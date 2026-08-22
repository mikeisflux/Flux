// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_FLOW_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_FLOW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/connectors/connector_registry.h"
#include "chrome/browser/flux/connectors/connector_tokens.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace flux {

// The client registration the user made with the provider.
//
// Flux does not ship client secrets. It cannot: a secret in a binary that
// anyone can download is not a secret, and several of these providers say so
// in their own docs. So each connector is registered by the user as their own
// app, and this is what they pasted.
struct OAuthClient {
  std::string client_id;
  std::string client_secret;  // empty for a public client using PKCE alone
  std::string redirect_uri;

  bool valid() const { return !client_id.empty() && !redirect_uri.empty(); }
};

// One in-flight authorization.
//
// PKCE is always used, including where the provider does not require it.
// The verifier costs nothing and the alternative - an authorization code that
// is useful to whoever else observes the redirect - is a real exposure in a
// browser, where the redirect lands in a tab that extensions can see.
class OAuthFlow {
 public:
  // `token` carries the access and refresh tokens on success. On failure it is
  // default-constructed and `error` says why, in words meant for the user.
  using CompleteCallback =
      base::OnceCallback<void(ConnectorToken token, const std::string& error)>;

  OAuthFlow(Profile* profile,
            const ConnectorDef& def,
            OAuthClient client);
  ~OAuthFlow();

  OAuthFlow(const OAuthFlow&) = delete;
  OAuthFlow& operator=(const OAuthFlow&) = delete;

  // The URL to send the user to. Generates and remembers the PKCE verifier and
  // the state parameter, so it must be called exactly once per flow.
  GURL BuildAuthorizeUrl();

  // True when `url` is this flow's redirect carrying a result. Matching is on
  // scheme, host, port and path only: the query is where the result is, and a
  // provider may add parameters of its own.
  bool IsRedirect(const GURL& url) const;

  // Handles the redirect. Verifies `state`, then exchanges the code.
  void HandleRedirect(const GURL& url, CompleteCallback callback);

  // Refreshes an existing token. Static because it does not need a live flow -
  // no user interaction is involved.
  static void Refresh(Profile* profile,
                      const ConnectorDef& def,
                      const OAuthClient& client,
                      const ConnectorToken& current,
                      CompleteCallback callback);

  const std::string& state() const { return state_; }

 private:
  void ExchangeCode(const std::string& code, CompleteCallback callback);

  raw_ptr<Profile> profile_;
  const ConnectorDef& def_;
  OAuthClient client_;
  std::string code_verifier_;
  std::string state_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<OAuthFlow> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_OAUTH_FLOW_H_
