// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_ask_button.h"

#include "base/functional/bind.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/text_constants.h"
// GetViewAccessibility() returns a reference to a type view.h only forward
// declares, so calling SetName() on it needs the real header. Same include
// the avatar button carries, for the same line.
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace flux {
namespace {

// An active pill is black, not lime. The accent buys exactly three things in
// this product - the composer's send button, keyboard focus and the first-run
// flow - and spending it here would put a saturated block next to the
// connector marks that are supposed to be the only saturated pixels on screen.
constexpr SkColor kOnFill = SkColorSetRGB(0x14, 0x20, 0x0a);

// Washes over whatever the toolbar is painted with, rather than a colour of
// their own, so the pill sits in the row instead of on top of it.
constexpr SkColor kHoverFill = SkColorSetARGB(0x14, 0, 0, 0);
constexpr SkColor kPressedFill = SkColorSetARGB(0x28, 0, 0, 0);

}  // namespace

FluxAskButton::FluxAskButton(Profile* profile)
    : views::LabelButton(
          base::BindRepeating(&FluxAskButton::Toggle, base::Unretained(this)),
          u"Ask Flux"),
      profile_(profile) {
  // Unretained is safe: the callback belongs to this button, so it cannot
  // outlive it. Same pattern as the avatar.
  SetTooltipText(u"Ask Flux about this page");
  GetViewAccessibility().SetName(u"Ask Flux");
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHeight / 2);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAskPanelOpen,
      base::BindRepeating(&FluxAskButton::OnOpenChanged,
                          base::Unretained(this)));
  OnOpenChanged();
}

FluxAskButton::~FluxAskButton() = default;

gfx::Size FluxAskButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kWidth, kHeight);
}

void FluxAskButton::OnPaintBackground(gfx::Canvas* canvas) {
  // A pill with no pressed state reads as a label, and the first version was
  // exactly that: it worked, and nothing on screen said so.
  SkColor fill = SK_ColorTRANSPARENT;
  if (IsPanelOpen()) {
    fill = kOnFill;
  } else if (GetState() == STATE_PRESSED) {
    fill = kPressedFill;
  } else if (GetState() == STATE_HOVERED) {
    fill = kHoverFill;
  }
  if (fill == SK_ColorTRANSPARENT) {
    return;
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(fill);
  const gfx::RectF bounds(GetLocalBounds());
  canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
}

void FluxAskButton::StateChanged(ButtonState old_state) {
  views::LabelButton::StateChanged(old_state);
  SchedulePaint();
}

bool FluxAskButton::IsPanelOpen() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kAskPanelOpen);
}

void FluxAskButton::OnOpenChanged() {
  // The label has to invert with the fill: near-black text on a near-black
  // pill is an invisible button that is nonetheless doing its job.
  const SkColor text = IsPanelOpen() ? SK_ColorWHITE : kOnFill;
  for (ButtonState state :
       {STATE_NORMAL, STATE_HOVERED, STATE_PRESSED, STATE_DISABLED}) {
    SetTextColor(state, text);
  }
  SchedulePaint();
}

void FluxAskButton::Toggle() {
  // The pref is what the layout reads, so opening and closing both go through
  // it - the panel's own close button sets the same one. Two controls for one
  // piece of state, and only one place that state lives.
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kAskPanelOpen,
                    !prefs->GetBoolean(prefs::kAskPanelOpen));
}

BEGIN_METADATA(FluxAskButton)
END_METADATA

}  // namespace flux
