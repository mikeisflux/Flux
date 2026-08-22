// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_avatar_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/text_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace flux {

namespace {

// Every row is a destination, so the model is a table rather than a switch.
// Deliberately no IDC commands: half of them would need verifying against a
// command table that moves, and a URL cannot silently become a no-op.
enum MenuCommand {
  kProfiles = 1,
  kAddProfile,
  kBookmarks,
  kHistory,
  kDownloads,
  kPasswords,
  kExtensions,
  kFeedback,
  kSource,
  kSettings,
};

const char* CommandUrl(int command_id) {
  switch (command_id) {
    case kProfiles:
    case kAddProfile:
      return "chrome://settings/manageProfile";
    case kBookmarks:
      return chrome::kChromeUIBookmarksURL;
    case kHistory:
      return chrome::kChromeUIHistoryURL;
    case kDownloads:
      return chrome::kChromeUIDownloadsURL;
    case kPasswords:
      return chrome::kChromeUIPasswordManagerURL;
    case kExtensions:
      return chrome::kChromeUIExtensionsURL;
    case kFeedback:
      return "https://github.com/mikeisflux/Flux/issues";
    case kSource:
      return "https://github.com/mikeisflux/Flux";
    case kSettings:
      return "chrome://flux/#settings";
    default:
      return nullptr;
  }
}

}  // namespace

FluxAvatarButton::FluxAvatarButton(BrowserWindowInterface* browser)
    : views::Button(base::BindRepeating(&FluxAvatarButton::ShowMenu,
                                        base::Unretained(this))),
      browser_(browser) {
  SetTooltipText(u"Profile and browser");
  GetViewAccessibility().SetName(u"Profile and browser");

  menu_model_.AddTitle(u"Profiles");
  menu_model_.AddItem(kProfiles, u"Manage profiles");
  menu_model_.AddItem(kAddProfile, u"Add profile");

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddTitle(u"Browser");
  menu_model_.AddItem(kBookmarks, u"Bookmarks");
  menu_model_.AddItem(kHistory, u"History");
  menu_model_.AddItem(kDownloads, u"Downloads");
  menu_model_.AddItem(kPasswords, u"Passwords");
  menu_model_.AddItem(kExtensions, u"Extensions");

  // The reference also lists Refer a Friend and Join Slack. Refer a Friend
  // needs a referral system that does not exist, and a Slack invite needs a
  // workspace that does not exist; a menu row that goes nowhere is worse than
  // a shorter menu, so neither is here. The section is filled out instead with
  // the destinations that are real today.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddTitle(u"Community");
  menu_model_.AddItem(kFeedback, u"Feedback and bugs");
  menu_model_.AddItem(kSource, u"Flux on GitHub");

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItem(kSettings, u"Settings");
}

FluxAvatarButton::~FluxAvatarButton() = default;

gfx::Size FluxAvatarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kSize, kSize);
}

std::u16string FluxAvatarButton::Initials() const {
  const std::u16string name =
      base::UTF8ToUTF16(browser_->GetProfile()->GetProfileUserName());
  if (name.empty()) {
    return u"F";
  }
  return std::u16string(1, base::ToUpperASCII(name[0]));
}

void FluxAvatarButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  // Flux lime, matching the mark. The avatar is the only place the accent
  // appears in the frame, which is what makes it findable without a label.
  flags.setColor(SkColorSetRGB(0xb4, 0xf0, 0x3c));
  const gfx::RectF bounds(GetContentsBounds());
  canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, flags);

  canvas->DrawStringRectWithFlags(
      Initials(), gfx::FontList().DeriveWithSizeDelta(-1),
      SkColorSetRGB(0x14, 0x20, 0x0a), GetContentsBounds(),
      gfx::Canvas::TEXT_ALIGN_CENTER);
}

void FluxAvatarButton::ShowMenu() {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      &menu_model_, views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), /*button_controller=*/nullptr,
                          GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kMouse);
}

void FluxAvatarButton::ExecuteCommand(int command_id, int event_flags) {
  if (const char* url = CommandUrl(command_id)) {
    browser_->OpenGURL(GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
}

BEGIN_METADATA(FluxAvatarButton)
END_METADATA

}  // namespace flux
