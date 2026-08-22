// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_REDIRECT_WATCHER_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_REDIRECT_WATCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace flux {

class ConnectorService;

// Watches the tab an authorization was opened in, and catches the redirect.
//
// A desktop app has to run a loopback HTTP server to receive an OAuth
// redirect. A browser does not: the redirect is a navigation in a tab it
// already owns, so it can be read directly and the code never has to travel
// through a local socket. That is the one place being a browser makes this
// materially safer rather than merely different.
//
// Catches the navigation at start rather than after it commits. The redirect
// target is usually the user's own registered URI, which may be a dead
// localhost address or a placeholder that 404s; waiting for it to load means
// showing the user an error page with an authorization code in the address
// bar. Starting is enough - the query string is all that matters.
class OAuthRedirectWatcher : public content::WebContentsObserver {
 public:
  // Called once, with the tab already closed. `error` is empty on success.
  using DoneCallback = base::OnceCallback<void(const std::string& error)>;

  OAuthRedirectWatcher(content::WebContents* tab,
                       ConnectorService* service,
                       DoneCallback callback);
  ~OAuthRedirectWatcher() override;

  OAuthRedirectWatcher(const OAuthRedirectWatcher&) = delete;
  OAuthRedirectWatcher& operator=(const OAuthRedirectWatcher&) = delete;

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void WebContentsDestroyed() override;

 private:
  void OnConnectComplete(bool ok, const std::string& error);
  void Finish(const std::string& error);
  void CloseTab();

  base::WeakPtr<content::WebContents> tab_;
  raw_ptr<ConnectorService> service_;
  DoneCallback callback_;
  bool finished_ = false;
  base::WeakPtrFactory<OAuthRedirectWatcher> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_REDIRECT_WATCHER_H_
