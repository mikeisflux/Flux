// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_browser_view_layout.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/flux/ui/flux_avatar_button.h"
// For the complete type: BrowserViewLayoutViews only forward-declares
// TabStripRegionView, and converting that pointer to views::View* to look its
// bounds up needs the definition.
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

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
    trailing.content.set_width(trailing.content.width() + AvatarSlot());
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

  // The avatar sits in the tab strip's band, between the last tab and the
  // caption buttons, vertically centred on the strip. Its bounds come from the
  // strip the base already placed rather than from a guess at the band height,
  // which changes with the frame, the theme and fullscreen.
  if (views().flux_avatar) {
    gfx::Rect bounds;
    const bool visible = HasAvatar();
    if (visible) {
      const ProposedLayout* strip =
          layout.GetLayoutFor(views().horizontal_tab_strip_region_view);
      gfx::Rect band = strip ? strip->bounds : params.visual_client_area;
      const int caption = base::ClampCeil(
          params.trailing_exclusion.ContentWithPadding().width());
      const int size = FluxAvatarButton::kSize;
      bounds = gfx::Rect(
          band.right() - caption - kAvatarGap - size,
          band.y() + std::max(0, (band.height() - size) / 2), size, size);
    }
    layout.AddChild(views().flux_avatar, bounds, visible);
  }

  return layout;
}

}  // namespace flux
