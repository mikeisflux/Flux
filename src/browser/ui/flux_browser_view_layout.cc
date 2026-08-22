// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_browser_view_layout.h"

#include <utility>

#include "chrome/browser/flux/ui/flux_sidebar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace flux {

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

gfx::Size FluxBrowserViewLayout::GetMinimumSize(const views::View* host) const {
  gfx::Size size = BrowserViewTabbedLayoutImpl::GetMinimumSize(host);
  if (HasSidebar()) {
    // The base measured the window as if the whole width were available to it,
    // so its answer is the minimum for everything to the right of the sidebar.
    size.Enlarge(FluxSidebarView::kWidth, 0);
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
    inset.InsetHorizontal(FluxSidebarView::kWidth, /*leading=*/true);
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
    bounds.set_width(FluxSidebarView::kWidth);
    layout.AddChild(views().flux_sidebar, bounds, HasSidebar());
  }

  return layout;
}

}  // namespace flux
