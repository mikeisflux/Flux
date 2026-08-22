// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_sidebar_view.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace flux {

namespace {

// The console serves the shell's nav from its own host, so the sidebar is the
// same WebUI as the tab content - one data source, one Mojo interface, one
// stylesheet. Requesting the file directly avoids needing a route for it.
GURL SidebarURL() {
  return GURL(chrome::kChromeUIFluxURL).Resolve("sidebar.html");
}

}  // namespace

FluxSidebarView::FluxSidebarView(BrowserWindowInterface* browser)
    : browser_(browser),
      contents_wrapper_(std::make_unique<WebUIContentsWrapperT<FluxUI>>(
          SidebarURL(),
          browser->GetProfile(),
          IDS_PRODUCT_NAME,
          // Escape belongs to the page the user is looking at. The sidebar is
          // permanent chrome; there is nothing for Escape to close.
          /*esc_closes_ui=*/false)) {
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());

  // Lets the console reach the window it is drawn in - which tab is active,
  // what the user is looking at - without the sidebar being a tab itself.
  webui::SetBrowserWindowInterface(contents_wrapper_->web_contents(), browser);

  SetVisible(true);
}

FluxSidebarView::~FluxSidebarView() = default;

void FluxSidebarView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::WebView::ViewHierarchyChanged(details);
  // Without this the contents stay in the hidden lifecycle state and never
  // paint, because nothing else ever shows them - the sidebar has no open or
  // close event of its own.
  if (details.is_add && details.child == this) {
    web_contents()->WasShown();
  }
}

void FluxSidebarView::ShowUI() {
  // Already showing, and always will be.
}

void FluxSidebarView::CloseUI() {
  // The sidebar is not closable. A page that asks is ignored rather than
  // obeyed: this view is frame furniture, and its lifetime belongs to the
  // window.
}

bool FluxSidebarView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

content::WebContents* FluxSidebarView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // A link in the sidebar navigates the window, not the sidebar. The nav items
  // are target=_blank precisely so they arrive here: a same-frame navigation
  // never consults the delegate, and would replace the sidebar with the page.
  browser_->OpenGURL(params.url, WindowOpenDisposition::CURRENT_TAB);
  return nullptr;
}

BEGIN_METADATA(FluxSidebarView)
END_METADATA

}  // namespace flux
