// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/oauth_flow.h"

#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "crypto/sha2.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace flux {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("flux_connector_oauth", R"FLUX(
      semantics {
        sender: "Flux Connectors"
        description:
          "Exchanges an OAuth authorization code for an access token, or "
          "refreshes an expired access token, with a third-party service the "
          "user has chosen to connect."
        trigger:
          "The user connects a service on the Flux connectors screen, or an "
          "agent task uses a connector whose access token has expired."
        data:
          "The authorization code or refresh token, and the OAuth client id "
          "and secret the user registered with that service."
        destination: OTHER
        destination_other: "The token endpoint of the service being connected."
      }
      policy {
        cookies_allowed: NO
        setting:
          "Disabled unless the user connects a service on the connectors "
          "screen."
      })FLUX");

// A URL-safe random string. Used for both the PKCE verifier and the state
// parameter, which have the same requirement: unguessable, and legal in a
// query string without further escaping.
std::string RandomUrlSafeString(size_t bytes) {
  std::string raw = base::RandBytesAsString(bytes);
  std::string encoded;
  base::Base64UrlEncode(raw, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

// RFC 7636 S256: BASE64URL(SHA256(ASCII(verifier))), unpadded.
std::string CodeChallengeFor(const std::string& verifier) {
  const std::string digest = crypto::SHA256HashString(verifier);
  std::string encoded;
  base::Base64UrlEncode(digest, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

// Scope delimiter. Space is the RFC 6749 default and what almost everything
// wants; the exceptions are recorded per connector.
//
// Linear is the one in the catalogue that is documented as comma-separated. A
// scope string joined with the wrong delimiter is not rejected - the provider
// reads it as one long nonsense scope and issues a token that is missing
// everything, which surfaces much later as a 403 on an unrelated call.
std::string JoinScopes(const std::string& connector_id,
                       const std::vector<std::string>& scopes) {
  const std::string separator = connector_id == "linear" ? "," : " ";
  return base::JoinString(scopes, separator);
}

// Parses the RFC 6749 token response, which is the same shape for an initial
// exchange and for a refresh.
ConnectorToken ParseTokenResponse(const std::string& body,
                                  const ConnectorToken& previous,
                                  std::string* error) {
  ConnectorToken token;

  std::optional<base::Value> parsed = base::JSONReader::Read(body);
  if (!parsed || !parsed->is_dict()) {
    *error = "The service's token endpoint did not return JSON.";
    return token;
  }
  const base::DictValue& dict = parsed->GetDict();

  // RFC 6749 error response. Prefer the human-readable description where the
  // provider bothered to send one.
  if (const std::string* err = dict.FindString("error")) {
    const std::string* description = dict.FindString("error_description");
    *error = description && !description->empty()
                 ? *description
                 : base::StrCat({"The service rejected the request: ", *err});
    return token;
  }

  const std::string* access = dict.FindString("access_token");
  if (!access || access->empty()) {
    *error = "The service's response contained no access token.";
    return token;
  }
  token.access_token = *access;

  if (const std::string* refresh = dict.FindString("refresh_token")) {
    token.refresh_token = *refresh;
  } else {
    // A refresh response is allowed to omit the refresh token, meaning "keep
    // using the one you have". Dropping it here would make the connection
    // un-refreshable on the next cycle and look like a random disconnect a
    // fortnight later.
    token.refresh_token = previous.refresh_token;
  }

  // expires_in is seconds from now. Some providers send it as a string.
  std::optional<double> expires_in = dict.FindDouble("expires_in");
  if (!expires_in) {
    if (const std::string* s = dict.FindString("expires_in")) {
      double value = 0;
      if (base::StringToDouble(*s, &value))
        expires_in = value;
    }
  }
  if (expires_in && *expires_in > 0)
    token.expires_at = base::Time::Now() + base::Seconds(*expires_in);

  token.base_url = previous.base_url;
  token.extra = previous.extra;
  return token;
}

// Posts an application/x-www-form-urlencoded body to a token endpoint.
void PostToTokenEndpoint(Profile* profile,
                         const ConnectorDef& def,
                         const OAuthClient& client,
                         std::vector<std::pair<std::string, std::string>> form,
                         const ConnectorToken& previous,
                         std::unique_ptr<network::SimpleURLLoader>* owner,
                         OAuthFlow::CompleteCallback callback) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(def.auth.token_url);
  request->method = "POST";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader("Content-Type",
                             "application/x-www-form-urlencoded");
  request->headers.SetHeader("Accept", "application/json");

  // HTTP Basic for the providers that require it (Ramp is the one in the
  // catalogue that says so outright), the credentials in the body otherwise.
  // Sending both is not harmless: some servers reject a request that
  // authenticates two ways as ambiguous.
  if (def.auth.client_auth == "basic") {
    request->headers.SetHeader(
        "Authorization",
        base::StrCat({"Basic ",
                      base::Base64Encode(base::StrCat(
                          {client.client_id, ":", client.client_secret}))}));
  } else {
    form.emplace_back("client_id", client.client_id);
    if (!client.client_secret.empty())
      form.emplace_back("client_secret", client.client_secret);
  }

  std::string body;
  for (const auto& [key, value] : form) {
    if (!body.empty())
      body += "&";
    base::StrAppend(&body, {base::EscapeUrlEncodedData(key, /*use_plus=*/true),
                            "=",
                            base::EscapeUrlEncodedData(value, true)});
  }

  *owner = network::SimpleURLLoader::Create(std::move(request),
                                            kTrafficAnnotation);
  (*owner)->AttachStringForUpload(body, "application/x-www-form-urlencoded");
  (*owner)->SetTimeoutDuration(base::Seconds(30));
  (*owner)->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  // The body is wanted even on 4xx: that is where the error description is,
  // and "the service rejected it" without saying why is a support ticket.
  (*owner)->SetAllowHttpErrorResults(true);

  network::SimpleURLLoader* loader = owner->get();
  loader->DownloadToString(
      profile->GetURLLoaderFactory().get(),
      base::BindOnce(
          [](ConnectorToken previous, OAuthFlow::CompleteCallback callback,
             std::optional<std::string> response) {
            if (!response) {
              std::move(callback).Run(
                  ConnectorToken(),
                  "Could not reach the service's token endpoint.");
              return;
            }
            std::string error;
            ConnectorToken token =
                ParseTokenResponse(*response, previous, &error);
            std::move(callback).Run(std::move(token), error);
          },
          previous, std::move(callback)),
      /*max_body_size=*/64 * 1024);
}

}  // namespace

OAuthFlow::OAuthFlow(Profile* profile,
                     const ConnectorDef& def,
                     OAuthClient client)
    : profile_(profile), def_(def), client_(std::move(client)) {}

OAuthFlow::~OAuthFlow() = default;

GURL OAuthFlow::BuildAuthorizeUrl() {
  // 32 bytes is the RFC 7636 recommendation; base64url of it lands inside the
  // 43-128 character range the spec requires of a verifier.
  code_verifier_ = RandomUrlSafeString(32);
  state_ = RandomUrlSafeString(16);

  GURL url(def_.auth.authorize_url);
  if (!url.is_valid())
    return GURL();

  url = net::AppendQueryParameter(url, "response_type", "code");
  url = net::AppendQueryParameter(url, "client_id", client_.client_id);
  url = net::AppendQueryParameter(url, "redirect_uri", client_.redirect_uri);
  url = net::AppendQueryParameter(url, "state", state_);
  url = net::AppendQueryParameter(url, "code_challenge",
                                  CodeChallengeFor(code_verifier_));
  url = net::AppendQueryParameter(url, "code_challenge_method", "S256");

  // An empty scope list is meaningful, not missing: Notion has no scope
  // parameter at all, and Basecamp's authorize endpoint does not take one.
  // Sending scope= empty is not the same as omitting it and some providers
  // reject it.
  if (!def_.auth.scopes.empty()) {
    url = net::AppendQueryParameter(url, "scope",
                                    JoinScopes(def_.id, def_.auth.scopes));
  }
  return url;
}

bool OAuthFlow::IsRedirect(const GURL& url) const {
  const GURL redirect(client_.redirect_uri);
  if (!url.is_valid() || !redirect.is_valid())
    return false;
  return url.scheme() == redirect.scheme() && url.host() == redirect.host() &&
         url.EffectiveIntPort() == redirect.EffectiveIntPort() &&
         url.path() == redirect.path();
}

void OAuthFlow::HandleRedirect(const GURL& url, CompleteCallback callback) {
  std::string value;

  // The provider reports a refusal on the redirect too, and it is the most
  // common outcome after "user clicked deny".
  if (net::GetValueForKeyInQuery(url, "error", &value)) {
    std::string description;
    net::GetValueForKeyInQuery(url, "error_description", &description);
    std::move(callback).Run(
        ConnectorToken(),
        description.empty() ? base::StrCat({"Authorization failed: ", value})
                            : description);
    return;
  }

  std::string returned_state;
  net::GetValueForKeyInQuery(url, "state", &returned_state);
  // Constant-time is not required - the state is single-use and compared once
  // - but the check itself is: without it, an attacker's authorization code
  // can be swapped in and Flux stores a token for the attacker's account.
  if (returned_state != state_ || state_.empty()) {
    std::move(callback).Run(
        ConnectorToken(),
        "The authorization response did not match the request. Nothing was "
        "connected. Try again from the connectors screen.");
    return;
  }

  std::string code;
  if (!net::GetValueForKeyInQuery(url, "code", &code) || code.empty()) {
    std::move(callback).Run(ConnectorToken(),
                            "The service did not return an authorization "
                            "code.");
    return;
  }

  ExchangeCode(code, std::move(callback));
}

void OAuthFlow::ExchangeCode(const std::string& code,
                             CompleteCallback callback) {
  std::vector<std::pair<std::string, std::string>> form = {
      {"grant_type", "authorization_code"},
      {"code", code},
      {"redirect_uri", client_.redirect_uri},
      {"code_verifier", code_verifier_},
  };
  PostToTokenEndpoint(profile_, def_, client_, std::move(form),
                      ConnectorToken(), &loader_, std::move(callback));
}

// static
void OAuthFlow::Refresh(Profile* profile,
                        const ConnectorDef& def,
                        const OAuthClient& client,
                        const ConnectorToken& current,
                        CompleteCallback callback) {
  if (current.refresh_token.empty()) {
    std::move(callback).Run(
        ConnectorToken(),
        "This connection has expired and cannot be renewed automatically. "
        "Reconnect it on the connectors screen.");
    return;
  }

  std::vector<std::pair<std::string, std::string>> form = {
      {"grant_type", "refresh_token"},
      {"refresh_token", current.refresh_token},
  };

  // The loader has to outlive this call; a refresh has no flow object to hang
  // it on, so it is kept alive by the callback it is bound into.
  auto owner = std::make_unique<std::unique_ptr<network::SimpleURLLoader>>();
  auto* raw_owner = owner.get();
  PostToTokenEndpoint(
      profile, def, client, std::move(form), current, raw_owner,
      base::BindOnce(
          [](std::unique_ptr<std::unique_ptr<network::SimpleURLLoader>> keep,
             CompleteCallback callback, ConnectorToken token,
             const std::string& error) {
            std::move(callback).Run(std::move(token), error);
          },
          std::move(owner), std::move(callback)));
}

}  // namespace flux
