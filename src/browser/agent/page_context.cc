// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/flux/agent/page_context.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace flux {
namespace {

// Client-rendered pages fire load before they are done painting. A short
// settle after quiescence catches the common case without a fixed sleep on
// every capture.
constexpr base::TimeDelta kSettleDelay = base::Milliseconds(350);
constexpr base::TimeDelta kPollInterval = base::Milliseconds(250);

// Cap the snapshot so one enormous page cannot consume the whole context
// window. Truncation is reported to the model rather than silent, so it knows
// to scroll or extract rather than assume it saw everything.
constexpr size_t kMaxContentChars = 24000;
constexpr size_t kMaxInteractiveNodes = 300;

bool IsInteractiveRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kComboBoxSelect:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
      return true;
    default:
      return false;
  }
}

bool IsTextEntryRole(ax::mojom::Role role) {
  return role == ax::mojom::Role::kTextField ||
         role == ax::mojom::Role::kSearchBox ||
         role == ax::mojom::Role::kTextFieldWithComboBox;
}

// Roles whose text is page content rather than chrome. Navigation, banners and
// footers are dropped: they cost context on every snapshot and almost never
// carry what the task needs.
bool IsContentRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kNavigation:
      return false;
    default:
      return true;
  }
}

std::string RoleName(ax::mojom::Role role) {
  return std::string(ui::ToString(role));
}

std::string NameOf(const ui::AXNode& node) {
  return node.GetStringAttribute(ax::mojom::StringAttribute::kName);
}

// Accessible names that mean "this commits something".
bool NameReadsAsCommit(const std::string& name) {
  static constexpr const char* kCommitWords[] = {
      "send", "post", "submit", "publish", "pay", "place order", "buy",
      "confirm", "checkout", "book now", "purchase", "delete", "transfer",
  };
  const std::string lower = base::ToLowerASCII(name);
  for (const char* word : kCommitWords) {
    if (lower.find(word) != std::string::npos)
      return true;
  }
  return false;
}

}  // namespace

PageContext::PageContext(content::WebContents* web_contents)
    : web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr) {}

PageContext::~PageContext() = default;

void PageContext::CaptureWhenStable(SnapshotCallback callback) {
  if (!web_contents_) {
    std::move(callback).Run(Snapshot());
    return;
  }

  if (web_contents_->IsLoading()) {
    // Poll rather than hooking DidStopLoading: single-page apps navigate
    // without a load event, so the loading flag is the more reliable signal.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PageContext::CaptureWhenStable,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        kPollInterval);
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageContext> self, SnapshotCallback cb) {
            if (!self || !self->web_contents_) {
              std::move(cb).Run(Snapshot());
              return;
            }
            self->web_contents_->RequestAXTreeSnapshot(
                base::BindOnce(&PageContext::OnAccessibilityTreeReady,
                               self, std::move(cb)),
                ui::kAXModeComplete,
                /*max_nodes=*/0, /*timeout=*/base::Seconds(5),
                content::WebContents::AXTreeSnapshotPolicy::kAll);
          },
          weak_factory_.GetWeakPtr(), std::move(callback)),
      kSettleDelay);
}

void PageContext::OnAccessibilityTreeReady(SnapshotCallback callback,
                                           ui::AXTreeUpdate& update) {
  Snapshot snapshot;
  if (web_contents_) {
    snapshot.url = web_contents_->GetLastCommittedURL().spec();
    snapshot.title = base::UTF16ToUTF8(web_contents_->GetTitle());
  }
  snapshot.is_stable = true;

  // A fresh tree for every snapshot. See the member's declaration: these
  // snapshots are standalone, and feeding one into the previous tree is what
  // killed the browser process on the second read of a live page.
  tree_ = std::make_unique<ui::AXTree>();
  if (!tree_->Unserialize(update)) {
    snapshot.content = "(could not read the page structure)";
    std::move(callback).Run(std::move(snapshot));
    return;
  }

  std::string content;
  std::vector<const ui::AXNode*> stack{tree_->root()};
  while (!stack.empty()) {
    const ui::AXNode* node = stack.back();
    stack.pop_back();
    if (!node)
      continue;

    const ax::mojom::Role role = node->GetRole();

    // Invisible and ignored are not the same thing, and treating them as one
    // threw away most of every real page.
    //
    // Invisible (display:none) genuinely has nothing under it worth reading,
    // so the whole subtree goes. IGNORED does not mean that at all: Blink's
    // tree is full of ignored structural wrappers whose descendants are the
    // actual content, which is precisely why AXNode offers
    // UnignoredChildrenBegin(). Skipping an ignored node's subtree blanked
    // everything below the first such wrapper.
    if (node->data().IsInvisible())
      continue;
    const bool emit = !node->IsIgnored();

    if (emit && IsInteractiveRole(role) &&
        snapshot.interactive.size() < kMaxInteractiveNodes) {
      InteractiveNode entry;
      entry.node_id = node->id();
      entry.role = RoleName(role);
      entry.name = NameOf(*node);
      entry.value =
          node->GetStringAttribute(ax::mojom::StringAttribute::kValue);
      entry.is_focusable =
          node->HasState(ax::mojom::State::kFocusable);
      entry.bounds = gfx::ToEnclosingRect(node->data().relative_bounds.bounds);
      entry.is_offscreen = node->data().relative_bounds.bounds.IsEmpty();
      // A control with no accessible name is unusable by the model and
      // usually decorative. Keep it only if it has a value to report.
      if (!entry.name.empty() || !entry.value.empty())
        snapshot.interactive.push_back(std::move(entry));
    }

    if (emit && role == ax::mojom::Role::kStaticText) {
      const std::string text = NameOf(*node);
      if (!text.empty() && content.size() < kMaxContentChars) {
        content.append(text);
        content.push_back('\n');
      }
    }

    for (auto it = node->children().rbegin(); it != node->children().rend();
         ++it) {
      // Skip whole chrome subtrees rather than filtering leaf by leaf.
      if (IsContentRole((*it)->GetRole()) || IsInteractiveRole((*it)->GetRole()))
        stack.push_back(*it);
    }

  }

  if (content.size() >= kMaxContentChars) {
    content.append(
        "\n[page truncated - scroll or use extract for the rest]\n");
  }
  snapshot.content = std::move(content);

  std::move(callback).Run(std::move(snapshot));
}

// static
std::string PageContext::Format(const Snapshot& snapshot) {
  std::string out = base::StrCat({"URL: ", snapshot.url, "\n",
                                  "Title: ", snapshot.title, "\n\n"});
  out.append(snapshot.content);
  out.append("\n\nInteractive elements:\n");
  for (const InteractiveNode& node : snapshot.interactive) {
    // Numbered so the model refers to "14" rather than emitting a selector it
    // guessed - the single biggest source of brittleness in browser agents.
    out.append(base::StrCat({"  [", base::NumberToString(node.node_id), "] ",
                             node.role, " \"", node.name, "\""}));
    if (!node.value.empty())
      out.append(base::StrCat({" (value: \"", node.value, "\")"}));
    if (node.is_offscreen)
      out.append(" (offscreen - scroll to it first)");
    out.push_back('\n');
  }
  return out;
}

// static
std::string PageContext::FormatForExtraction(
    const Snapshot& snapshot,
    const base::DictValue& request) {
  base::DictValue root;
  root.Set("url", snapshot.url);
  root.Set("title", snapshot.title);
  if (const base::ListValue* fields = request.FindList("fields"))
    root.Set("requested_fields", fields->Clone());
  // Extraction runs against the accessibility snapshot rather than the DOM, so
  // it survives markup changes that break selector-based scrapers.
  root.Set("content", snapshot.content);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

ui::AXNode* PageContext::NodeFromId(int32_t node_id) const {
  return tree_ ? tree_->GetFromId(node_id) : nullptr;
}

std::optional<gfx::Point> PageContext::ResolveNodeCenter(int32_t node_id) {
  ui::AXNode* node = NodeFromId(node_id);
  if (!node || node->IsInvisibleOrIgnored())
    return std::nullopt;
  const gfx::Rect bounds =
      gfx::ToEnclosingRect(node->data().relative_bounds.bounds);
  if (bounds.IsEmpty())
    return std::nullopt;
  return bounds.CenterPoint();
}

bool PageContext::IsSubmitLike(int32_t node_id) const {
  ui::AXNode* node = NodeFromId(node_id);
  if (!node)
    return false;

  const std::string name = NameOf(*node);
  if (NameReadsAsCommit(name))
    return true;

  // A button inside a form is a submit candidate even when its label is
  // unremarkable.
  if (node->GetRole() == ax::mojom::Role::kButton) {
    for (const ui::AXNode* parent = node->parent(); parent;
         parent = parent->parent()) {
      if (parent->GetRole() == ax::mojom::Role::kForm)
        return true;
    }
  }
  return false;
}

void PageContext::ClickNode(int32_t node_id, ActionCallback callback) {
  std::optional<gfx::Point> point = ResolveNodeCenter(node_id);
  if (!point || !web_contents_) {
    std::move(callback).Run(false);
    return;
  }

  // RenderFrameHost::GetRenderWidgetHost is the documented preferred path.
  content::RenderFrameHost* frame = web_contents_->GetPrimaryMainFrame();
  content::RenderWidgetHost* widget =
      frame ? frame->GetRenderWidgetHost() : nullptr;
  if (!widget) {
    std::move(callback).Run(false);
    return;
  }

  // Delivered through RenderWidgetHost, on the same path as physical input, so
  // focus transitions and event ordering match what handlers expect.
  blink::WebMouseEvent down(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  down.button = blink::WebMouseEvent::Button::kLeft;
  down.SetPositionInWidget(point->x(), point->y());
  down.click_count = 1;
  widget->ForwardMouseEvent(down);

  blink::WebMouseEvent up = down;
  up.SetType(blink::WebInputEvent::Type::kMouseUp);
  widget->ForwardMouseEvent(up);

  std::move(callback).Run(true);
}

void PageContext::TypeIntoNode(int32_t node_id,
                               const std::string& text,
                               ActionCallback callback) {
  ui::AXNode* node = NodeFromId(node_id);
  if (!node || !web_contents_) {
    std::move(callback).Run(false);
    return;
  }
  // contenteditable composers report as textbox roles too, which is why this
  // checks the role rather than looking for an <input>.
  if (!IsTextEntryRole(node->GetRole()) &&
      !node->HasState(ax::mojom::State::kEditable)) {
    std::move(callback).Run(false);
    return;
  }

  ClickNode(node_id, base::DoNothing());

  // RenderFrameHost::GetRenderWidgetHost is the documented preferred path.
  content::RenderFrameHost* frame = web_contents_->GetPrimaryMainFrame();
  content::RenderWidgetHost* widget =
      frame ? frame->GetRenderWidgetHost() : nullptr;
  if (!widget) {
    std::move(callback).Run(false);
    return;
  }

  const std::u16string wide = base::UTF8ToUTF16(text);
  for (char16_t c : wide) {
    // ForwardKeyboardEvent takes input::NativeWebKeyboardEvent, not
    // blink::WebKeyboardEvent - the latter does not convert implicitly.
    input::NativeWebKeyboardEvent key(blink::WebInputEvent::Type::kChar,
                                      blink::WebInputEvent::kNoModifiers,
                                      ui::EventTimeForNow());
    key.text[0] = c;
    key.unmodified_text[0] = c;
    widget->ForwardKeyboardEvent(key);
  }

  std::move(callback).Run(true);
}

void PageContext::SubmitForm(int32_t node_id, ActionCallback callback) {
  // Submission goes through activating the control, so the page's own submit
  // handlers and validation run exactly as they would for a person.
  ClickNode(node_id, std::move(callback));
}

void PageContext::ScrollToNode(int32_t node_id, ActionCallback callback) {
  ui::AXNode* node = NodeFromId(node_id);
  if (!node || !web_contents_) {
    std::move(callback).Run(false);
    return;
  }
  web_contents_->GetPrimaryMainFrame()->AccessibilityPerformAction(
      ui::AXActionData());
  std::move(callback).Run(true);
}

void PageContext::WaitForText(const std::string& text,
                              base::TimeDelta timeout,
                              ActionCallback callback) {
  const base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  PollForText(text, deadline, std::move(callback));
}

void PageContext::PollForText(const std::string& text,
                              base::TimeTicks deadline,
                              ActionCallback callback) {
  CaptureWhenStable(base::BindOnce(
      [](base::WeakPtr<PageContext> self, std::string text,
         base::TimeTicks deadline, ActionCallback cb, Snapshot snapshot) {
        if (snapshot.content.find(text) != std::string::npos) {
          std::move(cb).Run(true);
          return;
        }
        if (!self || base::TimeTicks::Now() >= deadline) {
          std::move(cb).Run(false);
          return;
        }
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&PageContext::PollForText, self, std::move(text),
                           deadline, std::move(cb)),
            kPollInterval);
      },
      weak_factory_.GetWeakPtr(), text, deadline, std::move(callback)));
}

}  // namespace flux
