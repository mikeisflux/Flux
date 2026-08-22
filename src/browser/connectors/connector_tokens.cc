// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/connector_tokens.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/flux/flux_prefs.h"

namespace flux {
namespace {

// Refresh this far before the stated expiry. A token that expires while the
// request carrying it is in flight fails the request, and the agent reports
// that as the task failing rather than as a token to renew.
constexpr base::TimeDelta kRefreshMargin = base::Minutes(5);

}  // namespace

bool ConnectorToken::NeedsRefresh() const {
  if (expires_at.is_null())
    return false;
  return base::Time::Now() + kRefreshMargin >= expires_at;
}

ConnectorTokenStore::ConnectorTokenStore(Profile* profile)
    : store_(profile, prefs::kConnectorTokens) {}

ConnectorTokenStore::~ConnectorTokenStore() = default;

ConnectorToken ConnectorTokenStore::Get(const std::string& connector_id) const {
  ConnectorToken token;
  const std::string blob = store_.Get(connector_id);
  if (blob.empty())
    return token;

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(blob, base::JSON_PARSE_RFC);
  if (!parsed)
    return token;
  const base::DictValue& dict = *parsed;

  if (const std::string* v = dict.FindString("access_token"))
    token.access_token = *v;
  if (const std::string* v = dict.FindString("refresh_token"))
    token.refresh_token = *v;
  if (const std::string* v = dict.FindString("base_url"))
    token.base_url = *v;
  // Seconds since the epoch as a double: base::Time has no JSON form, and an
  // int would overflow the 2038 boundary on a 32-bit read of the same pref.
  if (std::optional<double> secs = dict.FindDouble("expires_at")) {
    token.expires_at = base::Time::FromSecondsSinceUnixEpoch(*secs);
  }
  if (const base::DictValue* extra = dict.FindDict("extra")) {
    for (const auto [key, value] : *extra) {
      if (value.is_string())
        token.extra[key] = value.GetString();
    }
  }
  return token;
}

bool ConnectorTokenStore::Set(const std::string& connector_id,
                              const ConnectorToken& token) {
  base::DictValue dict;
  dict.Set("access_token", token.access_token);
  if (!token.refresh_token.empty())
    dict.Set("refresh_token", token.refresh_token);
  if (!token.base_url.empty())
    dict.Set("base_url", token.base_url);
  if (!token.expires_at.is_null())
    dict.Set("expires_at", token.expires_at.InSecondsFSinceUnixEpoch());
  if (!token.extra.empty()) {
    base::DictValue extra;
    for (const auto& [key, value] : token.extra)
      extra.Set(key, value);
    dict.Set("extra", std::move(extra));
  }

  std::optional<std::string> json = base::WriteJson(dict);
  if (!json)
    return false;
  return store_.Set(connector_id, *json);
}

void ConnectorTokenStore::Clear(const std::string& connector_id) {
  store_.Clear(connector_id);
}

std::vector<std::string> ConnectorTokenStore::Connected() const {
  return store_.Names();
}

bool ConnectorTokenStore::IsConnected(const std::string& connector_id) const {
  return store_.Has(connector_id);
}

}  // namespace flux
