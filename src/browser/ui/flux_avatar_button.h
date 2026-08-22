// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_AVATAR_BUTTON_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_AVATAR_BUTTON_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/button/button.h"

class BrowserWindowInterface;

namespace views {
class MenuRunner;
}

namespace flux {

// The avatar at the trailing end of the titlebar, and the menu behind it.
//
// This is where the rest of the browser lives. The console's sidebar has no
// bookmarks, no history and no downloads, which reads as "this fork removed
// them" until you find this menu - so the menu is not a nicety, it is the only
// route to half of Chromium.
class FluxAvatarButton : public views::Button,
                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(FluxAvatarButton, views::Button)

 public:
  static constexpr int kSize = 28;

  explicit FluxAvatarButton(BrowserWindowInterface* browser);
  FluxAvatarButton(const FluxAvatarButton&) = delete;
  FluxAvatarButton& operator=(const FluxAvatarButton&) = delete;
  ~FluxAvatarButton() override;

  // views::View:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void ShowMenu();

  // The profile's initials, which is all the reference draws.
  std::u16string Initials() const;

  const raw_ptr<BrowserWindowInterface> browser_;
  ui::SimpleMenuModel menu_model_{this};
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_AVATAR_BUTTON_H_
