// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_sidebar_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "components/prefs/pref_service.h"
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

// The palette is a console screen, reached by fragment. It deliberately is not
// an overlay drawn over the page: an overlay over arbitrary web content is a
// separate always-on-top widget with its own focus and z-order problems, and
// the palette is a launcher - going to it is the point.
GURL PaletteURL() {
  return GURL(chrome::kChromeUIFluxURL).Resolve("#search");
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

  pref_change_registrar_.Init(browser->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSidebarCollapsed,
      base::BindRepeating(&FluxSidebarView::OnCollapsedChanged,
                          base::Unretained(this)));

  SetVisible(true);
}

int FluxSidebarView::CurrentWidth() const {
  return browser_->GetProfile()->GetPrefs()->GetBoolean(
             prefs::kSidebarCollapsed)
             ? kCollapsedWidth
             : kWidth;
}

gfx::Size FluxSidebarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Only the width is used - the layout gives this view the full height of the
  // window regardless.
  return gfx::Size(CurrentWidth(), 0);
}

void FluxSidebarView::OnCollapsedChanged() {
  PreferredSizeChanged();
  if (views::View* parent_view = parent()) {
    parent_view->InvalidateLayout();
  }
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

void FluxSidebarView::OpenCommandPalette() {
  // SINGLETON_TAB rather than a new tab: pressing Ctrl+K four times should
  // leave one console tab, not four. Chromium matches on the URL ignoring the
  // fragment, so an existing console tab is reused and re-navigated to the
  // palette wherever in the console it happened to be.
  browser_->OpenGURL(PaletteURL(), WindowOpenDisposition::SINGLETON_TAB);
}

content::WebContents* FluxSidebarView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // A link in the sidebar navigates the window, not the sidebar. The nav items
  // are target=_blank precisely so they arrive here: a same-frame navigation
  // never consults the delegate, and would replace the sidebar with the page.
  //
  // The requested disposition is honoured rather than forced to CURRENT_TAB.
  // Connecting a service needs its authorization page in a tab of its own that
  // Flux can then watch and close, and replacing whatever the user was looking
  // at with an OAuth consent screen is its own bug besides.
  const WindowOpenDisposition disposition =
      params.disposition == WindowOpenDisposition::UNKNOWN
          ? WindowOpenDisposition::CURRENT_TAB
          : params.disposition;
  browser_->OpenGURL(params.url, disposition);

  // BrowserWindowInterface::OpenGURL returns void, so the new contents has to
  // be read back off the tab strip. Only meaningful for a foreground tab,
  // which has just become the active one; for anything else the caller gets
  // nullptr, which is what this always used to return.
  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    if (TabStripModel* tabs = browser_->GetTabStripModel())
      return tabs->GetActiveWebContents();
  }
  return nullptr;
}

BEGIN_METADATA(FluxSidebarView)
END_METADATA

}  // namespace flux
