// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_SERVICE_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/connectors/connector_client.h"
#include "chrome/browser/flux/connectors/connector_registry.h"
#include "chrome/browser/flux/connectors/connector_tokens.h"
#include "chrome/browser/flux/connectors/oauth_flow.h"
#include "chrome/browser/flux/secret_store.h"

class Profile;

namespace flux {

// What the console shows for one connector.
struct ConnectorStatus {
  std::string id;
  bool connectable = false;   // there is an auth path Flux can drive
  bool has_client = false;    // the user registered an OAuth app
  bool connected = false;
  bool expired = false;
  std::string detail;         // why it is not connectable, when it is not
};

// Everything the browser process knows about connectors, in one place.
//
// Owns the registry (shared, read-only), the token store, the client
// registrations, and the HTTP client. One per profile, held by
// FluxAgentService.
class ConnectorService {
 public:
  using ConnectCallback =
      base::OnceCallback<void(bool ok, const std::string& error)>;

  explicit ConnectorService(Profile* profile);
  ~ConnectorService();

  ConnectorService(const ConnectorService&) = delete;
  ConnectorService& operator=(const ConnectorService&) = delete;

  const ConnectorRegistry& registry() const { return registry_; }
  ConnectorTokenStore* tokens() { return &tokens_; }

  std::vector<ConnectorStatus> ListStatus() const;
  ConnectorStatus GetStatus(const std::string& connector_id) const;

  // The OAuth app the user registered with the provider. Flux ships no client
  // secrets - it cannot keep one - so each connector is the user's own app.
  bool SetClient(const std::string& connector_id, const OAuthClient& client);
  OAuthClient GetClient(const std::string& connector_id) const;
  void ClearClient(const std::string& connector_id);

  // The URL to open for the user to authorize. Empty when the connector has no
  // OAuth path or no registered client; `error` says which.
  GURL BeginConnect(const std::string& connector_id, std::string* error);

  // True when `url` completes an authorization Flux started.
  bool IsPendingRedirect(const GURL& url) const;

  // Completes it: verifies state, exchanges the code, runs the connector's
  // account-discovery step if it has one, and stores the token.
  void CompleteConnect(const GURL& url, ConnectCallback callback);

  // A pasted personal token, for the connectors that offer one.
  bool SetPersonalToken(const std::string& connector_id,
                        const std::string& token);

  void Disconnect(const std::string& connector_id);

  // Runs an operation, refreshing the token first if it has expired. This is
  // the entry point the agent's tools use; ConnectorClient::Execute is the
  // layer below and does not refresh.
  void Execute(ConnectorClient::Request request,
               ConnectorClient::ResponseCallback callback);

  // Fetches a next-page URL the provider itself handed back. Separate from
  // Execute because the URL is already complete: re-deriving it from an
  // operation path would be guesswork, and Basecamp's docs ask outright that
  // clients not build pagination URLs themselves.
  void FollowPage(const std::string& connector_id,
                  const std::string& url,
                  mojom::WriteScope granted_scope,
                  ConnectorClient::ResponseCallback callback);

 private:
  void OnTokenReceived(std::string connector_id,
                       ConnectCallback callback,
                       ConnectorToken token,
                       const std::string& error);
  void RunDiscovery(std::string connector_id,
                    ConnectorToken token,
                    ConnectCallback callback);
  void OnDiscoveryResponse(std::string connector_id,
                           ConnectorToken token,
                           ConnectCallback callback,
                           ConnectorResponse response);
  void OnRefreshed(ConnectorClient::Request request,
                   ConnectorClient::ResponseCallback callback,
                   ConnectorToken token,
                   const std::string& error);

  raw_ptr<Profile> profile_;
  ConnectorRegistry registry_;
  ConnectorTokenStore tokens_;
  SecretStore clients_;
  ConnectorClient client_;

  // At most one authorization at a time. A second Connect while one is open
  // replaces it: two live flows means two `state` values and a redirect that
  // could be matched against the wrong one.
  std::unique_ptr<OAuthFlow> pending_flow_;
  std::string pending_connector_;

  base::WeakPtrFactory<ConnectorService> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_SERVICE_H_
