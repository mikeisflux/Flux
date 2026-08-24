// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_UI_FLUX_ASK_PANEL_VIEW_H_
#define CHROME_BROWSER_FLUX_UI_FLUX_ASK_PANEL_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/webui/flux_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;

namespace flux {

// The Ask Flux conversation, beside the page.
//
// Unlike FluxSidebarView this one is closable and starts closed, so it is
// laid out against the contents area rather than the whole window: the tab
// strip and the toolbar run the full width above it. What it is not is a tab -
// the conversation has to survive switching tabs, and a page must not be able
// to navigate or dismiss it.
class FluxAskPanelView : public views::WebView,
                         public WebUIContentsWrapper::Host {
  METADATA_HEADER(FluxAskPanelView, views::WebView)

 public:
  // Wide enough for prose at a readable measure and narrow enough that the
  // page beside it is still usable, which is the whole bargain of a panel.
  static constexpr int kWidth = 380;

  explicit FluxAskPanelView(BrowserWindowInterface* browser);
  FluxAskPanelView(const FluxAskPanelView&) = delete;
  FluxAskPanelView& operator=(const FluxAskPanelView&) = delete;
  ~FluxAskPanelView() override;

  // Whether the window should be reserving room for it right now.
  bool IsOpen() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;

 private:
  void OnOpenChanged();

  const raw_ptr<BrowserWindowInterface> browser_;
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  PrefChangeRegistrar pref_change_registrar_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<FluxAskPanelView> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_UI_FLUX_ASK_PANEL_VIEW_H_
