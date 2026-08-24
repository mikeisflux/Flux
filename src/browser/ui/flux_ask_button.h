// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

class Profile;

namespace flux {

// "Ask Flux", at the trailing end of the toolbar row.
//
// It sits immediately left of the three-dot menu, which is where the reference
// product puts it and, more importantly, is client area: the titlebar band is
// not. A view there answers WM_NCHITTEST with HTCAPTION, Windows begins a
// window drag, and no click is ever delivered - which is exactly what happened
// to the first version of this button.
class FluxAskButton : public views::LabelButton {
  METADATA_HEADER(FluxAskButton, views::LabelButton)

 public:
  // A pill, so it reads as a thing to press rather than as an icon in a row of
  // icons. Height matches the avatar so the band has one baseline.
  static constexpr int kHeight = 28;
  static constexpr int kWidth = 96;

  explicit FluxAskButton(Profile* profile);
  FluxAskButton(const FluxAskButton&) = delete;
  FluxAskButton& operator=(const FluxAskButton&) = delete;
  ~FluxAskButton() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  // Defined in the .cc, not here: chromium-style rejects a virtual with a
  // non-empty body declared inline in a header.
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // views::Button:
  void StateChanged(ButtonState old_state) override;

 private:
  void Toggle();

  // Whether the panel is open right now. The pill is a toggle, so it has to
  // draw an on state, and the pref is the one place that state lives - the
  // panel's own close button sets it too.
  bool IsPanelOpen() const;
  void OnOpenChanged();

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_ASK_BUTTON_H_
