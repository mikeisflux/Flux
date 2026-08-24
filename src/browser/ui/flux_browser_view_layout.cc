// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_browser_view_layout.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/flux/ui/flux_ask_button.h"
#include "chrome/browser/flux/ui/flux_ask_panel_view.h"
#include "chrome/browser/flux/ui/flux_avatar_button.h"
// For the complete type: BrowserViewLayoutViews only forward-declares
// TabStripRegionView, and converting that pointer to views::View* to look its
// bounds up needs the definition.
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace flux {

namespace {

// Between the avatar and the last tab on one side, and the caption buttons on
// the other. Enough that it does not read as part of either.
constexpr int kAvatarGap = 8;

}  // namespace

FluxBrowserViewLayout::FluxBrowserViewLayout(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewTabbedLayoutImpl(std::move(delegate), browser,
                                  std::move(views)) {}

FluxBrowserViewLayout::~FluxBrowserViewLayout() = default;

bool FluxBrowserViewLayout::HasSidebar() const {
  return IsParentedTo(views().flux_sidebar, views().browser_view);
}

// Asked of the view rather than read from the constant, because the user can
// collapse it to the icon rail and the whole window has to follow.
int FluxBrowserViewLayout::SidebarWidth() const {
  return views().flux_sidebar->GetPreferredSize().width();
}

bool FluxBrowserViewLayout::HasAvatar() const {
  return IsParentedTo(views().flux_avatar, views().browser_view);
}

int FluxBrowserViewLayout::AvatarSlot() const {
  return FluxAvatarButton::kSize + 2 * kAvatarGap;
}

bool FluxBrowserViewLayout::HasAskButton() const {
  return IsParentedTo(views().flux_ask_button, views().browser_view);
}

int FluxBrowserViewLayout::AskButtonSlot() const {
  return FluxAskButton::kWidth + kAvatarGap;
}

bool FluxBrowserViewLayout::IsAskPanelOpen() const {
  const auto* panel =
      views::AsViewClass<FluxAskPanelView>(views().flux_ask_panel);
  return panel && IsParentedTo(views().flux_ask_panel, views().browser_view) &&
         panel->IsOpen();
}

gfx::Size FluxBrowserViewLayout::GetMinimumSize(const views::View* host) const {
  gfx::Size size = BrowserViewTabbedLayoutImpl::GetMinimumSize(host);
  if (HasSidebar()) {
    // The base measured the window as if the whole width were available to it,
    // so its answer is the minimum for everything to the right of the sidebar.
    size.Enlarge(SidebarWidth(), 0);
  }
  return size;
}

void FluxBrowserViewLayout::DoPreLayoutComputations(
    const BrowserLayoutParams& params) {
  // This is the whole mechanism. Everything downstream reads the params stored
  // here rather than the ones passed to CalculateProposedLayout(), so narrowing
  // them once, here, moves the entire browser over by the sidebar's width -
  // tab strip included, which is the part a side panel could never do.
  //
  // InsetHorizontal() also shrinks the leading exclusion, which is correct: the
  // frame's own controls sit at the window's edge, and the region the tab strip
  // is now laid out in no longer touches it.
  BrowserLayoutParams inset = params;
  if (HasSidebar()) {
    inset.InsetHorizontal(SidebarWidth(), /*leading=*/true);
  }
  if (HasAvatar()) {
    // Widening the trailing exclusion rather than insetting: the exclusion is
    // what the tab strip measures itself against, and it only applies to the
    // band the caption buttons are in - insetting would pull the contents area
    // in for the whole height of the window.
    auto& trailing = inset.trailing_exclusion;
    trailing.content.set_width(trailing.content.width() + AvatarSlot() +
                               (HasAskButton() ? AskButtonSlot() : 0));
    trailing.content.set_height(
        std::max<float>(trailing.content.height(), FluxAvatarButton::kSize));
  }
  BrowserViewTabbedLayoutImpl::DoPreLayoutComputations(inset);
}

// Trailing return type on purpose: ProposedLayout is protected in the base, so
// naming it ahead of the function would be looked up at namespace scope and
// rejected. After the declarator it resolves in this class's scope.
auto FluxBrowserViewLayout::CalculateProposedLayout(
    const BrowserLayoutParams& params) const -> ProposedLayout {
  ProposedLayout layout =
      BrowserViewTabbedLayoutImpl::CalculateProposedLayout(params);

  // `params` here is still the full window - the base class works from the
  // narrowed copy stashed by DoPreLayoutComputations() - so this is the gap
  // that was reserved, from the very top of the client area to the bottom.
  //
  // Every child of the browser view has to appear in the layout or ApplyLayout
  // reports it as an orphan, so this runs whether or not the sidebar is
  // showing.
  if (views().flux_sidebar) {
    gfx::Rect bounds = params.visual_client_area;
    bounds.set_width(SidebarWidth());
    layout.AddChild(views().flux_sidebar, bounds, HasSidebar());
  }

  // The avatar sits in the slot DoPreLayoutComputations reserved for it, which
  // is the AvatarSlot()-wide strip of the caption band immediately inboard of
  // the window controls.
  //
  // Positioned from the tab strip's own right edge, not by subtracting the
  // caption width from it. GetBoundsWithExclusion() gives the strip a width of
  // `visual_client_area.width() - (leading + trailing)`, and `trailing` is the
  // exclusion already widened by AvatarSlot() - so the strip's right edge IS
  // the slot's left edge, and subtracting the caption again counted it twice
  // and parked the avatar a caption's width out into the tab strip.
  //
  // Vertical placement comes from the strip the base already laid out rather
  // than from a guess at the band height, which changes with the frame, the
  // theme and fullscreen.
  if (views().flux_avatar) {
    gfx::Rect bounds;
    const bool visible = HasAvatar();
    if (visible) {
      const int size = FluxAvatarButton::kSize;
      const ProposedLayout* strip =
          layout.GetLayoutFor(views().horizontal_tab_strip_region_view);
      int slot_left;
      int band_top;
      int band_height;
      if (strip) {
        slot_left = strip->bounds.right();
        band_top = strip->bounds.y();
        band_height = strip->bounds.height();
      } else {
        // No horizontal strip - a vertical tab strip, or a frame that has not
        // placed one yet. The slot is still there, so measure it back from the
        // window edge using the un-widened exclusion `params` still carries.
        const gfx::SizeF caption =
            params.trailing_exclusion.ContentWithPadding();
        slot_left = params.visual_client_area.right() -
                    base::ClampCeil(caption.width()) - AvatarSlot();
        band_top = params.visual_client_area.y();
        band_height = std::max(base::ClampCeil(caption.height()), size);
      }
      bounds = gfx::Rect(slot_left + kAvatarGap,
                         band_top + std::max(0, (band_height - size) / 2),
                         size, size);
    }
    layout.AddChild(views().flux_avatar, bounds, visible);

    // The pill goes immediately inboard of the avatar, in the slot widened
    // for it above. Measured from the avatar's own bounds rather than
    // recomputed, so the two cannot disagree about where the band is.
    if (views().flux_ask_button) {
      gfx::Rect pill;
      if (visible && HasAskButton()) {
        pill = gfx::Rect(
            bounds.x() - kAvatarGap - FluxAskButton::kWidth,
            bounds.y() + (FluxAvatarButton::kSize - FluxAskButton::kHeight) / 2,
            FluxAskButton::kWidth, FluxAskButton::kHeight);
      }
      layout.AddChild(views().flux_ask_button, pill,
                      visible && HasAskButton());
    }
  }

  // The panel takes its width off the contents area rather than off the
  // window. Unlike the sidebar it sits beside the page only: the tab strip and
  // the toolbar run the full width above it, which is what makes it read as a
  // panel over the content rather than as a second column of chrome.
  //
  // Done by shrinking what the base already laid out, because the base is what
  // knows where the contents area ends up once the toolbar, bookmarks bar and
  // any side panel have had their say.
  if (views().flux_ask_panel) {
    const bool open = IsAskPanelOpen();
    gfx::Rect bounds;
    if (open) {
      if (views::ChildLayout* contents =
              layout.GetLayoutFor(views().contents_container)) {
        const int width =
            std::min(FluxAskPanelView::kWidth, contents->bounds.width());
        bounds = contents->bounds;
        bounds.set_x(contents->bounds.right() - width);
        bounds.set_width(width);
        contents->bounds.set_width(contents->bounds.width() - width);
      }
    }
    layout.AddChild(views().flux_ask_panel, bounds, open && !bounds.IsEmpty());
  }

  return layout;
}

}  // namespace flux
