// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

class BrowserWindowInterface;

namespace flux {

// "Ask Flux", in the titlebar band beside the avatar.
//
// It sits here rather than in the toolbar row on purpose. The band is already
// laid out by FluxBrowserViewLayout, so this costs one slot in code this fork
// owns; putting it in the toolbar would mean a patch against toolbar_view.cc,
// a file Chromium rewrites constantly, for a few pixels of position.
class FluxAskButton : public views::LabelButton {
  METADATA_HEADER(FluxAskButton, views::LabelButton)

 public:
  // A pill, so it reads as a thing to press rather than as an icon in a row of
  // icons. Height matches the avatar so the band has one baseline.
  static constexpr int kHeight = 28;
  static constexpr int kWidth = 96;

  explicit FluxAskButton(BrowserWindowInterface* browser);
  FluxAskButton(const FluxAskButton&) = delete;
  FluxAskButton& operator=(const FluxAskButton&) = delete;
  ~FluxAskButton() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void Toggle();

  const raw_ptr<BrowserWindowInterface> browser_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_
