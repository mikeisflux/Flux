// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_TOKENS_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_TOKENS_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/flux/secret_store.h"

class Profile;

namespace flux {

// What Flux holds for one connected service.
struct ConnectorToken {
  std::string access_token;
  std::string refresh_token;

  // Null when the provider did not say. Absence means "no expiry known", NOT
  // "expired" - Notion's tokens do not expire and treating a missing value as
  // stale would send every request through a refresh that has no refresh
  // token to use.
  base::Time expires_at;

  // Set once from the provider's own discovery document where it has one.
  // Basecamp is why this exists: its API base URL is the href out of the
  // authorization document, and building it from the account id instead is
  // explicitly not the contract.
  std::string base_url;

  // Anything else the connector needs to make a request and that is not
  // secret-shaped - an account id, a region, an instance host.
  std::map<std::string, std::string> extra;

  bool valid() const { return !access_token.empty(); }

  // True when the token is past its stated expiry, allowing for clock skew and
  // for the request itself taking time. Always false when no expiry is known.
  bool NeedsRefresh() const;
};

// Per-profile OAuth tokens and pasted API keys for connectors.
//
// Encrypted at rest through SecretStore, one JSON blob per connector id. The
// blob rather than a field per property because a token, its refresh token and
// its expiry are only meaningful together: a half-updated pair is worse than
// no token, and one encrypted value updated at once cannot half-update.
class ConnectorTokenStore {
 public:
  explicit ConnectorTokenStore(Profile* profile);
  ~ConnectorTokenStore();

  ConnectorTokenStore(const ConnectorTokenStore&) = delete;
  ConnectorTokenStore& operator=(const ConnectorTokenStore&) = delete;

  bool ready() const { return store_.ready(); }

  ConnectorToken Get(const std::string& connector_id) const;
  bool Set(const std::string& connector_id, const ConnectorToken& token);
  void Clear(const std::string& connector_id);

  // Which connectors have something stored. Does not decrypt, so the
  // connectors screen can render connected state before - or without - the
  // encryptor arriving.
  std::vector<std::string> Connected() const;
  bool IsConnected(const std::string& connector_id) const;

 private:
  SecretStore store_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_TOKENS_H_
