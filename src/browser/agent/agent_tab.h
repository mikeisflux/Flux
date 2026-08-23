// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_AGENT_TAB_H_
#define CHROME_BROWSER_FLUX_AGENT_AGENT_TAB_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace content {
class WebContents;
}

namespace flux {

// The tab a run acts in.
//
// A real tab, in the user's own window and their own profile - not an isolated
// context. That is the whole point of putting the agent inside the browser:
// the user is already signed in to Gmail, LinkedIn, their CRM and everything
// else, and an agent that has to authenticate for itself cannot do any of the
// work this product exists to do. Sharing the profile means sharing the cookie
// jar, and being signed in is the default rather than a feature.
//
// It is also visible on purpose. The user can watch the task happen and can
// take the tab over mid-run, which is the escape hatch for every case the
// agent gets wrong. Closing it ends the run rather than leaving the agent
// acting on a page nobody can see.
//
// The tab outlives the run. A finished task usually ends on the thing it
// produced, and closing that the instant the run completes would throw away
// the result the user asked for.
class AgentTab : public content::WebContentsObserver {
 public:
  // `closed` runs if the tab goes away before the run does.
  AgentTab(Profile* profile, base::OnceClosure closed);
  ~AgentTab() override;

  AgentTab(const AgentTab&) = delete;
  AgentTab& operator=(const AgentTab&) = delete;

  // Opens the tab and returns it, or null if no window would take it.
  content::WebContents* Open();

  // Null once the user has closed it.
  content::WebContents* contents() const;

 private:
  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  raw_ptr<Profile> profile_;
  base::OnceClosure closed_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_AGENT_TAB_H_
