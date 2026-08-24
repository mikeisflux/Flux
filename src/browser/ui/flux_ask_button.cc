// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_ask_button.h"

#include "base/functional/bind.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/prefs/pref_service.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
// GetViewAccessibility() returns a reference to a type view.h only forward
// declares, so calling SetName() on it needs the real header. Same include
// the avatar button carries, for the same line.
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace flux {

FluxAskButton::FluxAskButton(BrowserWindowInterface* browser)
    : views::LabelButton(
          base::BindRepeating(&FluxAskButton::Toggle, base::Unretained(this)),
          u"Ask Flux"),
      browser_(browser) {
  // Unretained is safe: the callback belongs to this button, so it cannot
  // outlive it. Same pattern as the avatar.
  SetTooltipText(u"Ask Flux about this page");
  GetViewAccessibility().SetName(u"Ask Flux");
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHeight / 2);
}

FluxAskButton::~FluxAskButton() = default;

gfx::Size FluxAskButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kWidth, kHeight);
}

void FluxAskButton::Toggle() {
  // The pref is what the layout reads, so opening and closing both go through
  // it - the panel's own close button sets the same one. Two controls for one
  // piece of state, and only one place that state lives.
  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAskPanelOpen,
                    !prefs->GetBoolean(prefs::kAskPanelOpen));
}

BEGIN_METADATA(FluxAskButton)
END_METADATA

}  // namespace flux
