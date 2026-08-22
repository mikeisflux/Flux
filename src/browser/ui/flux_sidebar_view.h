// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_SIDEBAR_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/webui/flux_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;

namespace flux {

// The column down the left of every window.
//
// This is browser chrome, not a tab and not a side panel. It starts at the top
// of the window - the tab strip begins to its right, not above it - it has no
// header, no close button and no entry in the side panel registry, and nothing
// a page does can navigate or dismiss it. FluxBrowserViewLayout takes its width
// off the window before anything else is measured.
class FluxSidebarView : public views::WebView,
                        public WebUIContentsWrapper::Host {
  METADATA_HEADER(FluxSidebarView, views::WebView)

 public:
  // Read off the reference design. Fixed rather than resizable: the console's
  // own layout is built around it, and the whole point of putting it in the
  // frame instead of the side panel was to stop it behaving like a panel.
  static constexpr int kWidth = 305;

  explicit FluxSidebarView(BrowserWindowInterface* browser);
  FluxSidebarView(const FluxSidebarView&) = delete;
  FluxSidebarView& operator=(const FluxSidebarView&) = delete;
  ~FluxSidebarView() override;

  // views::WebView:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<WebUIContentsWrapperT<FluxUI>> contents_wrapper_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<FluxSidebarView> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_SIDEBAR_VIEW_H_
