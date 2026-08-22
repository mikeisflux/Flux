// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/connector_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {
namespace {

// The one place the discovery response is turned into a base URL. Kept out of
// ConnectorClient because it is about interpreting a document, not about
// making a request.
std::string FindBaseUrl(const std::string& body,
                        const AccountDiscovery& discovery) {
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(body, base::JSON_PARSE_RFC);
  if (!parsed)
    return std::string();

  const base::ListValue* accounts = parsed->FindList(discovery.accounts_path);
  if (!accounts)
    return std::string();

  for (const base::Value& entry : *accounts) {
    if (!entry.is_dict())
      continue;
    const base::DictValue& account = entry.GetDict();

    bool matches = true;
    for (const auto& [key, expected] : discovery.match) {
      const std::string* actual = account.FindString(key);
      if (!actual || *actual != expected) {
        matches = false;
        break;
      }
    }
    if (!matches)
      continue;

    if (const std::string* href = account.FindString(discovery.base_url_field))
      return *href;
  }
  return std::string();
}

}  // namespace

ConnectorService::ConnectorService(Profile* profile)
    : profile_(profile),
      tokens_(profile),
      clients_(profile, prefs::kConnectorClients),
      client_(profile, &registry_, &tokens_) {}

ConnectorService::~ConnectorService() = default;

ConnectorStatus ConnectorService::GetStatus(
    const std::string& connector_id) const {
  ConnectorStatus status;
  status.id = connector_id;

  const ConnectorDef* def = registry_.Get(connector_id);
  if (!def) {
    status.detail = "No definition is shipped for this connector.";
    return status;
  }

  switch (def->auth.type) {
    case AuthType::kOAuth2:
    case AuthType::kApiKey:
      status.connectable = true;
      break;
    case AuthType::kLocal:
      status.detail = "Reads from this machine - nothing to connect.";
      break;
    case AuthType::kMcp:
      status.detail = "Reached over MCP. Configure the MCP server instead.";
      break;
    case AuthType::kUnsupported:
      status.detail = "This service has no API Flux can use.";
      break;
  }

  status.has_client = !GetClient(connector_id).client_id.empty();
  status.connected = tokens_.IsConnected(connector_id);
  if (status.connected)
    status.expired = tokens_.Get(connector_id).NeedsRefresh();

  if (status.connectable && def->auth.type == AuthType::kOAuth2 &&
      !status.has_client) {
    status.detail =
        "Register an OAuth app with this service and paste its client id, "
        "secret and redirect URI. Flux ships no client secrets of its own.";
  }
  return status;
}

std::vector<ConnectorStatus> ConnectorService::ListStatus() const {
  std::vector<ConnectorStatus> out;
  for (const ConnectorDef* def : registry_.All())
    out.push_back(GetStatus(def->id));
  return out;
}

bool ConnectorService::SetClient(const std::string& connector_id,
                                 const OAuthClient& client) {
  base::DictValue dict;
  dict.Set("client_id", client.client_id);
  dict.Set("client_secret", client.client_secret);
  dict.Set("redirect_uri", client.redirect_uri);
  std::optional<std::string> json = base::WriteJson(dict);
  return json && clients_.Set(connector_id, *json);
}

OAuthClient ConnectorService::GetClient(const std::string& connector_id) const {
  OAuthClient client;
  const std::string blob = clients_.Get(connector_id);
  if (blob.empty())
    return client;
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(blob, base::JSON_PARSE_RFC);
  if (!parsed)
    return client;
  const base::DictValue& dict = *parsed;
  if (const std::string* v = dict.FindString("client_id"))
    client.client_id = *v;
  if (const std::string* v = dict.FindString("client_secret"))
    client.client_secret = *v;
  if (const std::string* v = dict.FindString("redirect_uri"))
    client.redirect_uri = *v;
  return client;
}

void ConnectorService::ClearClient(const std::string& connector_id) {
  clients_.Clear(connector_id);
}

GURL ConnectorService::BeginConnect(const std::string& connector_id,
                                    std::string* error) {
  const ConnectorDef* def = registry_.Get(connector_id);
  if (!def) {
    *error = base::StrCat({"No connector called '", connector_id, "'."});
    return GURL();
  }
  if (def->auth.type != AuthType::kOAuth2) {
    *error = base::StrCat({def->id, " does not use OAuth."});
    return GURL();
  }

  const OAuthClient client = GetClient(connector_id);
  if (!client.valid()) {
    *error = base::StrCat(
        {"Register an OAuth app with ", def->id,
         " and add its client id and redirect URI before connecting."});
    return GURL();
  }

  pending_flow_ = std::make_unique<OAuthFlow>(profile_, *def, client);
  pending_connector_ = connector_id;

  GURL url = pending_flow_->BuildAuthorizeUrl();
  if (!url.is_valid()) {
    pending_flow_.reset();
    pending_connector_.clear();
    *error = base::StrCat({"The authorize URL for ", def->id, " is not valid: ",
                           def->auth.authorize_url});
  }
  return url;
}

bool ConnectorService::IsPendingRedirect(const GURL& url) const {
  return pending_flow_ && pending_flow_->IsRedirect(url);
}

void ConnectorService::CompleteConnect(const GURL& url,
                                       ConnectCallback callback) {
  if (!pending_flow_) {
    std::move(callback).Run(false, "No authorization is in progress.");
    return;
  }
  // Take the connector id now: the flow is destroyed as soon as it reports,
  // and the callback below needs to know which connector it was for.
  std::string connector_id = pending_connector_;
  pending_flow_->HandleRedirect(
      url, base::BindOnce(&ConnectorService::OnTokenReceived,
                          weak_factory_.GetWeakPtr(), std::move(connector_id),
                          std::move(callback)));
}

void ConnectorService::OnTokenReceived(std::string connector_id,
                                       ConnectCallback callback,
                                       ConnectorToken token,
                                       const std::string& error) {
  pending_flow_.reset();
  pending_connector_.clear();

  if (!error.empty() || !token.valid()) {
    std::move(callback).Run(
        false, error.empty() ? "No access token was returned." : error);
    return;
  }

  const ConnectorDef* def = registry_.Get(connector_id);
  if (def && def->auth.discovery.valid()) {
    RunDiscovery(std::move(connector_id), std::move(token),
                 std::move(callback));
    return;
  }

  const bool stored = tokens_.Set(connector_id, token);
  std::move(callback).Run(
      stored, stored ? std::string()
                     : "The token could not be stored securely, so it was "
                       "discarded. Nothing is connected.");
}

void ConnectorService::RunDiscovery(std::string connector_id,
                                    ConnectorToken token,
                                    ConnectCallback callback) {
  const ConnectorDef* def = registry_.Get(connector_id);
  CHECK(def);

  // The token is not stored yet, so ConnectorClient cannot be used - it reads
  // credentials from the store. Store it first, then discover, then update it
  // with the base URL. If discovery fails the token is dropped again: a
  // connection whose base URL had to be guessed is one that fails later in a
  // way nobody will connect back to this moment.
  if (!tokens_.Set(connector_id, token)) {
    std::move(callback).Run(false,
                            "The token could not be stored securely, so it "
                            "was discarded. Nothing is connected.");
    return;
  }

  client_.Follow(
      connector_id, def->auth.discovery.endpoint,
      mojom::WriteScope::kReadOnly,
      base::BindOnce(&ConnectorService::OnDiscoveryResponse,
                     weak_factory_.GetWeakPtr(), connector_id,
                     std::move(token), std::move(callback)));
}

void ConnectorService::OnDiscoveryResponse(std::string connector_id,
                                           ConnectorToken token,
                                           ConnectCallback callback,
                                           ConnectorResponse response) {
  const ConnectorDef* def = registry_.Get(connector_id);
  if (!def) {
    tokens_.Clear(connector_id);
    std::move(callback).Run(false, "The connector went away mid-connect.");
    return;
  }

  if (!response.ok()) {
    tokens_.Clear(connector_id);
    std::move(callback).Run(
        false, base::StrCat({"Connected, but could not read ", def->id,
                             "'s account list: ", response.error_message,
                             " Nothing was saved."}));
    return;
  }

  const std::string base_url =
      FindBaseUrl(response.body, def->auth.discovery);
  if (base_url.empty()) {
    tokens_.Clear(connector_id);
    std::move(callback).Run(
        false,
        base::StrCat({"Authorized, but no usable account was found in ",
                      def->id,
                      "'s response. Nothing was saved - connecting with a "
                      "guessed API host would fail later and less clearly."}));
    return;
  }

  token.base_url = base_url;
  const bool stored = tokens_.Set(connector_id, token);
  if (!stored)
    tokens_.Clear(connector_id);
  std::move(callback).Run(
      stored, stored ? std::string()
                     : "The token could not be stored securely.");
}

bool ConnectorService::SetPersonalToken(const std::string& connector_id,
                                        const std::string& token_value) {
  if (token_value.empty()) {
    tokens_.Clear(connector_id);
    return true;
  }
  ConnectorToken token;
  token.access_token = token_value;
  // No expiry: a pasted token is valid until the user revokes it, and
  // inventing one would send it through a refresh it has no refresh token for.
  return tokens_.Set(connector_id, token);
}

void ConnectorService::Disconnect(const std::string& connector_id) {
  tokens_.Clear(connector_id);
}

void ConnectorService::Execute(ConnectorClient::Request request,
                               ConnectorClient::ResponseCallback callback) {
  const ConnectorDef* def = registry_.Get(request.connector_id);
  const ConnectorToken token = tokens_.Get(request.connector_id);

  // Refresh before sending rather than after a 401. A 401 has already spent a
  // request against the rate limit, and on several of these services it also
  // counts toward a lockout.
  if (def && def->auth.type == AuthType::kOAuth2 && token.valid() &&
      token.NeedsRefresh()) {
    const OAuthClient client = GetClient(request.connector_id);
    if (client.valid()) {
      OAuthFlow::Refresh(
          profile_, *def, client, token,
          base::BindOnce(&ConnectorService::OnRefreshed,
                         weak_factory_.GetWeakPtr(), std::move(request),
                         std::move(callback)));
      return;
    }
  }

  client_.Execute(std::move(request), std::move(callback));
}

void ConnectorService::OnRefreshed(ConnectorClient::Request request,
                                   ConnectorClient::ResponseCallback callback,
                                   ConnectorToken token,
                                   const std::string& error) {
  if (!error.empty() || !token.valid()) {
    ConnectorResponse response;
    response.error = ConnectorError::kTokenExpired;
    response.error_message =
        error.empty()
            ? base::StrCat({"Could not renew the ", request.connector_id,
                            " connection."})
            : error;
    std::move(callback).Run(std::move(response));
    return;
  }

  // Carry the discovered base URL across the refresh. The token endpoint does
  // not repeat it, and losing it would send every subsequent request to the
  // definition's template with an unresolved placeholder in it.
  const ConnectorToken previous = tokens_.Get(request.connector_id);
  if (token.base_url.empty())
    token.base_url = previous.base_url;
  if (token.extra.empty())
    token.extra = previous.extra;

  tokens_.Set(request.connector_id, token);
  client_.Execute(std::move(request), std::move(callback));
}

}  // namespace flux
