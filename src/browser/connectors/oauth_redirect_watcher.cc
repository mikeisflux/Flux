// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/oauth_redirect_watcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/flux/connectors/connector_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace flux {

OAuthRedirectWatcher::OAuthRedirectWatcher(content::WebContents* tab,
                                           ConnectorService* service,
                                           DoneCallback callback)
    : content::WebContentsObserver(tab),
      tab_(tab ? tab->GetWeakPtr() : nullptr),
      service_(service),
      callback_(std::move(callback)) {}

OAuthRedirectWatcher::~OAuthRedirectWatcher() = default;

void OAuthRedirectWatcher::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (finished_ || !handle->IsInPrimaryMainFrame())
    return;

  const GURL& url = handle->GetURL();
  if (!service_ || !service_->IsPendingRedirect(url))
    return;

  // Stop watching before anything else: closing the tab below fires
  // WebContentsDestroyed, and that path reports an abandoned authorization.
  // Without this the user would be told they cancelled, moments before being
  // told it worked.
  Observe(nullptr);

  // The tab has done its job. Close it before the exchange rather than after:
  // the authorization code is in that address bar, and the exchange is a
  // round trip during which it sits there to be copied or read out of session
  // history.
  CloseTab();

  // Report only when the exchange has actually finished. Reporting on the
  // redirect instead would show "connected" while the token request is still
  // in flight, and a rejected exchange would never be reported at all.
  service_->CompleteConnect(
      url, base::BindOnce(&OAuthRedirectWatcher::OnConnectComplete,
                          weak_factory_.GetWeakPtr()));
}

void OAuthRedirectWatcher::OnConnectComplete(bool ok,
                                             const std::string& error) {
  Finish(ok ? std::string()
            : (error.empty() ? "Could not connect." : error));
}

void OAuthRedirectWatcher::WebContentsDestroyed() {
  // The user closed the tab. That is an abandoned authorization, not an error
  // worth a dialog, but the console has a spinner up and needs to stop.
  Finish("Authorization was cancelled.");
}

void OAuthRedirectWatcher::CloseTab() {
  if (!tab_)
    return;
  // Posted rather than called inline: this runs from inside a navigation
  // callback on the very WebContents being closed, and destroying it under
  // its own stack frame is a use-after-free.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<content::WebContents> contents) {
                       if (contents)
                         contents->ClosePage();
                     },
                     tab_));
  tab_ = nullptr;
}

void OAuthRedirectWatcher::Finish(const std::string& error) {
  if (finished_)
    return;
  finished_ = true;
  Observe(nullptr);
  if (callback_)
    std::move(callback_).Run(error);
}

}  // namespace flux
