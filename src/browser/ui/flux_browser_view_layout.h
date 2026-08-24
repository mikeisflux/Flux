// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_BROWSER_VIEW_LAYOUT_H_

#include <memory>

#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"

class Browser;
class BrowserViewLayoutDelegate;

namespace flux {

// The tabbed browser layout, with the console's column taken off the leading
// edge first.
//
// Chromium's layout is left completely intact and simply run inside a narrower
// window: DoPreLayoutComputations() insets the params it is handed, so the tab
// strip, toolbar, contents and side panel all measure and place themselves in
// the space that is left. That is what makes the tab strip start beside the
// sidebar rather than above it, and it is why this is a subclass rather than a
// patch - there is nothing to change in the base layout, only what it is told
// the window is.
class FluxBrowserViewLayout : public BrowserViewTabbedLayoutImpl {
 public:
  FluxBrowserViewLayout(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                        Browser* browser,
                        BrowserViewLayoutViews views);
  FluxBrowserViewLayout(const FluxBrowserViewLayout&) = delete;
  FluxBrowserViewLayout& operator=(const FluxBrowserViewLayout&) = delete;
  ~FluxBrowserViewLayout() override;

 protected:
  // BrowserViewTabbedLayoutImpl:
  gfx::Size GetMinimumSize(const views::View* host) const override;
  void DoPreLayoutComputations(const BrowserLayoutParams& params) override;
  ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const override;

 private:
  // False in windows that never got a sidebar, so this class stays safe to
  // install unconditionally for every tabbed browser.
  bool HasSidebar() const;

  // The width to reserve on the leading edge. Follows the collapsed state, so
  // it is asked of the view rather than taken from a constant.
  int SidebarWidth() const;

  bool HasAvatar() const;

  // What the avatar occupies at the trailing end of the tab strip band,
  // including the gap that keeps it off the caption buttons.
  int AvatarSlot() const;

  bool HasAskButton() const;

  // The Ask Flux pill's slot, immediately inboard of the avatar's.
  int AskButtonSlot() const;

  // Open, and therefore taking width off the contents area. Distinct from
  // "exists": the panel is built for every normal window and starts closed.
  bool IsAskPanelOpen() const;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_BROWSER_VIEW_LAYOUT_H_
