// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/agent_tab.h"

#include <utility>

#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace flux {

AgentTab::AgentTab(Profile* profile, base::OnceClosure closed)
    : profile_(profile), closed_(std::move(closed)) {}

AgentTab::~AgentTab() = default;

content::WebContents* AgentTab::Open() {
  if (!profile_)
    return nullptr;

  // The Profile overload rather than the BrowserWindowInterface one: Navigate()
  // will find a window for this profile or make one, which is the behaviour we
  // want for a scheduled run that fires with no window open.
  NavigateParams params(profile_, GURL(url::kAboutBlankURL),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  // Foreground, because a run the user cannot see is a run they cannot stop.
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);

  content::WebContents* contents = params.navigated_or_inserted_contents;
  if (!contents)
    return nullptr;

  // Observed rather than held as a bare pointer: the user can close this tab
  // at any moment, and a run still acting through a freed WebContents is the
  // worst possible version of that.
  Observe(contents);
  return contents;
}

content::WebContents* AgentTab::contents() const {
  return web_contents();
}

void AgentTab::WebContentsDestroyed() {
  Observe(nullptr);
  if (closed_)
    std::move(closed_).Run();
}

}  // namespace flux
