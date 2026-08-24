// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_
#define CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace flux {

// A page element the agent can act on, addressed by a stable id rather than a
// CSS selector. Selectors are the standard approach and they are why browser
// agents break on every site redesign; node ids come from the accessibility
// tree the browser already maintains for screen readers.
struct InteractiveNode {
  int32_t node_id;
  std::string role;         // button, textbox, link, checkbox...
  std::string name;         // accessible name, i.e. what a human would read
  std::string value;
  bool is_focusable = false;
  bool is_offscreen = false;
  gfx::Rect bounds;
};

// Serializes a live page into something a model can reason about.
//
// This is the single highest-leverage component in the agent. Feeding raw DOM
// wastes enormous context on markup the model does not need and still misses
// what is actually visible; feeding screenshots costs vision tokens and loses
// exact text. The accessibility tree is the browser's own semantic model of
// the page — it is what the page *means*, already computed, already handling
// shadow DOM, iframes and ARIA.
//
// Being inside Chromium rather than driving it from outside is what makes this
// cheap: no CDP round-trips, direct access to the AXTree the renderer already
// built.
class PageContext {
 public:
  struct Snapshot {
    std::string url;
    std::string title;
    // Readable text, already stripped of chrome, nav and boilerplate.
    std::string content;
    std::vector<InteractiveNode> interactive;
    // True when the page is still loading; the agent should wait rather than
    // act on a half-rendered tree.
    bool is_stable = false;
  };

  using SnapshotCallback = base::OnceCallback<void(Snapshot)>;

  explicit PageContext(content::WebContents* web_contents);
  ~PageContext();

  PageContext(const PageContext&) = delete;
  PageContext& operator=(const PageContext&) = delete;

  // Captures once the page is quiescent (load complete plus a short settle for
  // client-rendered content).
  void CaptureWhenStable(SnapshotCallback callback);

  // Renders a snapshot into the compact text form given to the model.
  // Interactive elements are numbered so the model can say "click 14" rather
  // than emitting a selector it guessed.
  static std::string Format(const Snapshot& snapshot);

  // Renders only the fields named in `request`, as JSON rows. Used by the
  // extract tool so tabular pages don't cost a full-page snapshot.
  static std::string FormatForExtraction(const Snapshot& snapshot,
                                         const base::DictValue& request);

  using ActionCallback = base::OnceCallback<void(bool success)>;

  // --- Interaction ---------------------------------------------------------
  //
  // These deliver input through RenderWidgetHost rather than by executing
  // JavaScript in the page. That is a correctness requirement:
  //
  //   - Rich-text editors are contenteditable, not <input>. Assigning .value
  //     does nothing, so the surrounding form submits empty. Every major mail
  //     and social composer works this way, and the failure is silent - the
  //     UI looks like it worked.
  //   - element.click() skips focus transitions and the mousedown/mouseup/
  //     click ordering that handlers bind to.
  //   - Frameworks with their own synthetic event systems frequently ignore
  //     JS-dispatched events, or update the DOM without updating component
  //     state, leaving the page and the model disagreeing about what happened.
  //
  // Delivering real input avoids all three without per-site special cases.

  // Clicks the element identified by `node_id` from the last snapshot.
  void ClickNode(int32_t node_id, ActionCallback callback);

  // Focuses `node_id` and enters `text` as key events. Paced rather than
  // delivered at once: composers that debounce input handling drop characters
  // that arrive faster than the debounce interval.
  void TypeIntoNode(int32_t node_id,
                    const std::string& text,
                    ActionCallback callback);

  void SubmitForm(int32_t node_id, ActionCallback callback);

  // True when `node_id` is a control whose activation submits or transmits -
  // a submit input, a button inside a form, or one whose accessible name
  // reads as a commit action ("Send", "Post", "Pay", "Place order").
  //
  // Scope-by-tool-name alone has an obvious hole: clicking is read-only, so
  // "click the Send button" would otherwise pass a read-only task unchecked.
  // AgentRunner::RequiresApproval consults this to escalate those clicks.
  bool IsSubmitLike(int32_t node_id) const;
  void ScrollToNode(int32_t node_id, ActionCallback callback);

  // Resolves once `text` appears, or `timeout` elapses. Preferable to a fixed
  // sleep after any action that triggers a load.
  void WaitForText(const std::string& text,
                   base::TimeDelta timeout,
                   ActionCallback callback);

 private:
  // Note: RequestAXTreeSnapshot's callback is
  // base::OnceCallback<void(ui::AXTreeUpdate&)> - a NON-const ref. Taking a
  // const ref here silently fails to bind.
  void OnAccessibilityTreeReady(SnapshotCallback callback,
                                ui::AXTreeUpdate& update);

  // Maps a node id to viewport coordinates, scrolling it into view first.
  // Returns nullopt when the node is gone - the page may have changed since
  // the snapshot the model is reasoning about, and acting on a stale id is how
  // an agent ends up clicking the wrong control.
  std::optional<gfx::Point> ResolveNodeCenter(int32_t node_id);

  void PollForText(const std::string& text,
                   base::TimeTicks deadline,
                   ActionCallback callback);

  base::WeakPtr<content::WebContents> web_contents_;
  // The tree behind the most recent snapshot. Node ids the model refers to are
  // resolved against this, so a stale id fails cleanly rather than hitting
  // whatever now occupies that position.
  // The node behind an id from the most recent snapshot, or null if there has
  // not been one yet.
  ui::AXNode* NodeFromId(int32_t node_id) const;

  // Replaced wholesale on every snapshot, never updated in place.
  //
  // RequestAXTreeSnapshot returns a complete standalone tree each time, not a
  // delta against the last one. Unserializing a second snapshot into a tree
  // that still holds the first is read as an incremental update, and on a live
  // application whose node ids have been reshuffled between captures that is
  // an illegal reparent: "Node 9 is not marked for destruction, would be
  // reparented to 3" - a FATAL inside AXTree, taking the browser process down
  // the first time the agent read Gmail twice.
  //
  // A unique_ptr because AXTree deletes copy and move assignment, so the only
  // way to replace one is to destroy it and build another. It is null until
  // the first snapshot, and GetFromId is reached from tool calls that can
  // arrive before one - hence NodeFromId rather than touching it directly.
  std::unique_ptr<ui::AXTree> tree_;
  base::WeakPtrFactory<PageContext> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_
