// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/ui/flux_ask_panel_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace flux {
namespace {

GURL PanelURL() {
  return GURL(chrome::kChromeUIFluxURL).Resolve("ask.html");
}

}  // namespace

FluxAskPanelView::FluxAskPanelView(BrowserWindowInterface* browser)
    : browser_(browser),
      contents_wrapper_(std::make_unique<WebUIContentsWrapperT<FluxUI>>(
          PanelURL(),
          browser->GetProfile(),
          IDS_PRODUCT_NAME,
          // Escape closes the panel. Unlike the sidebar this one is a thing
          // the user opened, so there is something for Escape to mean.
          /*esc_closes_ui=*/true)) {
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());
  webui::SetBrowserWindowInterface(contents_wrapper_->web_contents(), browser);

  pref_change_registrar_.Init(browser->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAskPanelOpen,
      base::BindRepeating(&FluxAskPanelView::OnOpenChanged,
                          base::Unretained(this)));

  SetVisible(IsOpen());
}

FluxAskPanelView::~FluxAskPanelView() = default;

bool FluxAskPanelView::IsOpen() const {
  return browser_->GetProfile()->GetPrefs()->GetBoolean(prefs::kAskPanelOpen);
}

gfx::Size FluxAskPanelView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Only the width is read; the layout gives it the height of the contents
  // area regardless.
  return gfx::Size(IsOpen() ? kWidth : 0, 0);
}

void FluxAskPanelView::OnOpenChanged() {
  SetVisible(IsOpen());
  PreferredSizeChanged();
  if (views::View* parent_view = parent()) {
    parent_view->InvalidateLayout();
  }
}

void FluxAskPanelView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::WebView::ViewHierarchyChanged(details);
  // Same reason as the sidebar: nothing else ever shows these contents, so
  // without this they stay in the hidden lifecycle state and never paint.
  if (details.is_add && details.child == this) {
    web_contents()->WasShown();
  }
}

void FluxAskPanelView::ShowUI() {
  browser_->GetProfile()->GetPrefs()->SetBoolean(prefs::kAskPanelOpen, true);
}

void FluxAskPanelView::CloseUI() {
  // The panel's own close button and Escape both arrive here. The pref is the
  // single source of truth for whether the window reserves room, so closing
  // goes through it rather than by hiding the view directly - the layout reads
  // the pref, not the visibility.
  browser_->GetProfile()->GetPrefs()->SetBoolean(prefs::kAskPanelOpen, false);
}

bool FluxAskPanelView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(FluxAskPanelView)
END_METADATA

}  // namespace flux
