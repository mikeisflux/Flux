// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/connector_client.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace flux {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("flux_connector_request", R"FLUX(
      semantics {
        sender: "Flux Connectors"
        description:
          "Makes an authorized API request to a third-party service the user "
          "has connected, on behalf of a running agent task."
        trigger:
          "An agent task uses a connected service - reading a project, "
          "posting a message, creating a to-do."
        data:
          "The access token for that service, and whatever the operation "
          "sends: message text, to-do content, search terms."
        destination: OTHER
        destination_other:
          "The API host of the service the user connected."
      }
      policy {
        cookies_allowed: NO
        setting:
          "Disabled unless the user connects a service on the connectors "
          "screen and runs a task that uses it."
      })FLUX");

int ScopeRank(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return 0;
    case mojom::WriteScope::kDraft:    return 1;
    case mojom::WriteScope::kSend:     return 2;
    case mojom::WriteScope::kPurchase: return 3;
  }
  return 3;
}

const char* ScopeName(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return "read-only";
    case mojom::WriteScope::kDraft:    return "draft";
    case mojom::WriteScope::kSend:     return "send";
    case mojom::WriteScope::kPurchase: return "purchase";
  }
  return "unknown";
}

// Pulls the rel="next" URL out of an RFC 5988 Link header.
//
// Parsed rather than constructed. Basecamp's pagination is "geared" - 15
// results on page 1, then 30, 50, 100 - so a page number appended by hand
// walks the wrong offsets, and its docs ask outright that clients not build
// these URLs. Every provider that sends a Link header means the same thing by
// it, so this is shared rather than per-connector.
std::string ParseNextLink(const std::string& header) {
  for (std::string_view part : base::SplitStringPiece(
           header, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    const size_t open = part.find('<');
    if (open == std::string_view::npos)
      continue;
    const size_t close = part.find('>', open + 1);
    if (close == std::string_view::npos)
      continue;
    // rel=next, rel="next" and rel='next' are all in the wild.
    std::string params(part.substr(close + 1));
    base::ReplaceChars(params, "\"'", "", &params);
    base::RemoveChars(params, " ", &params);
    if (params.find("rel=next") != std::string::npos)
      return std::string(part.substr(open + 1, close - open - 1));
  }
  return std::string();
}

// Retry-After is either seconds or an HTTP date. Only the seconds form is
// honoured: the date form needs the server's clock to agree with ours, and a
// skewed clock turns a 30-second wait into an hour or into no wait at all.
base::TimeDelta ParseRetryAfter(const std::string& value) {
  int seconds = 0;
  if (base::StringToInt(value, &seconds) && seconds > 0)
    return base::Seconds(seconds);
  return base::TimeDelta();
}

}  // namespace

// Everything one request needs to outlive the call that started it.
struct ConnectorClient::PendingRequest {
  std::string connector_id;
  GURL url;
  std::string method;
  std::string body;
  std::map<std::string, std::string> headers;
  std::unique_ptr<network::SimpleURLLoader> loader;
  ResponseCallback callback;
};

ConnectorClient::ConnectorClient(Profile* profile,
                                 const ConnectorRegistry* registry,
                                 ConnectorTokenStore* tokens)
    : profile_(profile), registry_(registry), tokens_(tokens) {}

ConnectorClient::~ConnectorClient() = default;

// static
bool ConnectorClient::ScopePermits(mojom::WriteScope granted,
                                   mojom::WriteScope required) {
  return ScopeRank(required) <= ScopeRank(granted);
}

void ConnectorClient::Execute(Request request, ResponseCallback callback) {
  auto fail = [&callback](ConnectorError error, std::string message) {
    ConnectorResponse response;
    response.error = error;
    response.error_message = std::move(message);
    std::move(callback).Run(std::move(response));
  };

  const ConnectorDef* def = registry_->Get(request.connector_id);
  if (!def) {
    fail(ConnectorError::kUnknownConnector,
         base::StrCat({"No connector called '", request.connector_id, "'."}));
    return;
  }

  const ConnectorOperation* op = def->FindOperation(request.operation);
  if (!op) {
    fail(ConnectorError::kUnknownOperation,
         base::StrCat({def->id, " has no operation called '",
                       request.operation, "'."}));
    return;
  }

  // The gate. Before the token is even read: a task that is not allowed to
  // send should not cause a token to be decrypted, let alone a request built.
  if (!ScopePermits(request.granted_scope, op->write_scope)) {
    fail(ConnectorError::kScopeRefused,
         base::StrCat({"Refused: ", def->id, ".", op->name, " is a ",
                       ScopeName(op->write_scope),
                       " operation and this task is limited to ",
                       ScopeName(request.granted_scope), "."}));
    return;
  }

  const ConnectorToken token = tokens_->Get(request.connector_id);
  if (!token.valid()) {
    fail(ConnectorError::kNotConnected,
         base::StrCat({def->id, " is not connected. Connect it on the "
                                "connectors screen."}));
    return;
  }
  // Refreshing needs the user's OAuth client registration, which the caller
  // owns; surfacing it as a distinct error lets the connector service refresh
  // and retry rather than burying the reason in an HTTP 401.
  if (token.NeedsRefresh()) {
    fail(ConnectorError::kTokenExpired,
         base::StrCat({"The ", def->id, " access token has expired."}));
    return;
  }

  // The base URL is whatever the provider's own discovery document said, and
  // only falls back to the definition's template when there was no discovery
  // step. Basecamp is the case that matters: its docs say the href from the
  // authorization document is the contract, not a URL built from the account
  // id.
  std::string base = token.base_url.empty() ? def->base_url : token.base_url;

  std::map<std::string, std::string> substitutions = request.path_params;
  for (const auto& [key, value] : token.extra)
    substitutions.emplace(key, value);

  const std::string path = ResolveTemplate(op->path, substitutions);
  std::string full = op->IsAbsolute()
                         ? path
                         : base::StrCat({ResolveTemplate(base, substitutions),
                                         path});

  // Checked before is_valid(), because is_valid() cannot see this. A leftover
  // {placeholder} does not make a URL invalid: url/url_canon_path.cc marks '{'
  // and '}' ESCAPE rather than reject, so GURL percent-encodes them and the
  // request goes out to /projects/%7Bproject_id%7D.json - a live call to a
  // real service, answered with a 404 that reads like the provider's fault.
  // Naming the parameter is the difference between a one-line fix and an
  // afternoon spent doubting the connector definition.
  if (std::optional<std::string> missing = FirstUnresolvedPlaceholder(full)) {
    fail(ConnectorError::kHttpError,
         base::StrCat({"Cannot call ", def->id, ".", op->name,
                       ": no value was supplied for {", *missing,
                       "}. The path is ", op->path,
                       ", so that parameter has to be passed in."}));
    return;
  }

  GURL url(full);
  if (!url.is_valid()) {
    fail(ConnectorError::kHttpError,
         base::StrCat({"Built an invalid URL for ", def->id, ".", op->name,
                       ": ", full, "."}));
    return;
  }
  for (const auto& [key, value] : request.query)
    url = net::AppendQueryParameter(url, key, value);

  auto pending = std::make_unique<PendingRequest>();
  pending->connector_id = request.connector_id;
  pending->url = std::move(url);
  pending->method = op->method.empty() ? "GET" : op->method;
  pending->body = std::move(request.body);
  pending->callback = std::move(callback);

  // Header templates come from the definition, so a mandatory header is data
  // rather than a special case in code. Basecamp's User-Agent is the reason:
  // omitting it is a 400 that says nothing about headers, and it is one line
  // in basecamp.json instead of a branch here.
  for (const auto& [name, value_template] : def->headers) {
    std::map<std::string, std::string> values = substitutions;
    values["access_token"] = token.access_token;
    values["api_token"] = token.access_token;
    pending->headers[name] = ResolveTemplate(value_template, values);
  }

  SendNow(std::move(pending));
}

void ConnectorClient::Follow(const std::string& connector_id,
                             const std::string& url,
                             mojom::WriteScope granted_scope,
                             ResponseCallback callback) {
  const ConnectorDef* def = registry_->Get(connector_id);
  const ConnectorToken token = tokens_->Get(connector_id);
  GURL target(url);

  if (!def || !token.valid() || !target.is_valid()) {
    ConnectorResponse response;
    response.error = def ? ConnectorError::kNotConnected
                         : ConnectorError::kUnknownConnector;
    response.error_message =
        base::StrCat({"Cannot follow ", url, " for ", connector_id, "."});
    std::move(callback).Run(std::move(response));
    return;
  }

  // Following a page is a read whatever the task is allowed to do, but the
  // gate is still consulted: a read-only task following a link is fine, and
  // this keeps the one-way-in property true of every request.
  if (!ScopePermits(granted_scope, mojom::WriteScope::kReadOnly)) {
    ConnectorResponse response;
    response.error = ConnectorError::kScopeRefused;
    response.error_message = "Refused: this task cannot read.";
    std::move(callback).Run(std::move(response));
    return;
  }

  auto pending = std::make_unique<PendingRequest>();
  pending->connector_id = connector_id;
  pending->url = std::move(target);
  pending->method = "GET";
  pending->callback = std::move(callback);
  for (const auto& [name, value_template] : def->headers) {
    std::map<std::string, std::string> values = token.extra;
    values["access_token"] = token.access_token;
    values["api_token"] = token.access_token;
    pending->headers[name] = ResolveTemplate(value_template, values);
  }

  SendNow(std::move(pending));
}

void ConnectorClient::SendNow(std::unique_ptr<PendingRequest> pending) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = pending->url;
  resource_request->method = pending->method;
  // The token is the credential. Sending the profile's cookies as well would
  // attach the user's logged-in session to an API call - a different identity
  // with, on several of these services, more authority than the token.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  for (const auto& [name, value] : pending->headers) {
    if (!value.empty())
      resource_request->headers.SetHeader(name, value);
  }

  pending->loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  if (!pending->body.empty()) {
    const auto it = pending->headers.find("Content-Type");
    pending->loader->AttachStringForUpload(
        pending->body,
        it == pending->headers.end() ? "application/json" : it->second);
  }
  pending->loader->SetTimeoutDuration(base::Seconds(60));
  pending->loader->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  // 4xx bodies carry the provider's error message, which is the only useful
  // thing to show the user when a call is rejected.
  pending->loader->SetAllowHttpErrorResults(true);

  network::SimpleURLLoader* loader = pending->loader.get();
  loader->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&ConnectorClient::OnResponse, weak_factory_.GetWeakPtr(),
                     std::move(pending)),
      // DownloadToString DCHECKs above kMaxBoundedStringDownloadSize. 8 MiB
      // was over it, and a connector response is nowhere near either.
      /*max_body_size=*/network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void ConnectorClient::OnResponse(std::unique_ptr<PendingRequest> pending,
                                 std::optional<std::string> body) {
  ConnectorResponse response;
  response.body = body.value_or(std::string());

  const network::mojom::URLResponseHead* head =
      pending->loader->ResponseInfo();
  if (!head || !head->headers) {
    response.error = ConnectorError::kNetworkError;
    response.error_message =
        base::StrCat({"Could not reach ", pending->url.host(), "."});
    std::move(pending->callback).Run(std::move(response));
    return;
  }

  response.http_status = head->headers->response_code();

  if (std::optional<std::string> link =
          head->headers->GetNormalizedHeader("Link")) {
    response.next_page_url = ParseNextLink(*link);
  }

  if (response.http_status == 429) {
    response.error = ConnectorError::kRateLimited;
    if (std::optional<std::string> retry =
            head->headers->GetNormalizedHeader("Retry-After")) {
      response.retry_after = ParseRetryAfter(*retry);
    }
    response.error_message =
        base::StrCat({pending->connector_id, " is rate limiting this "
                                             "connection."});
    std::move(pending->callback).Run(std::move(response));
    return;
  }

  // Basecamp marks a suspended or expired account with a Reason header on an
  // otherwise ordinary 404, and asks that clients disable the connection
  // rather than retry. Nothing else in the catalogue uses this header, so
  // reading it unconditionally costs nothing and gets that case right.
  if (response.http_status == 404) {
    std::optional<std::string> reason =
        head->headers->GetNormalizedHeader("Reason");
    if (reason && base::EqualsCaseInsensitiveASCII(*reason,
                                                   "Account Inactive")) {
      response.error = ConnectorError::kAccountInactive;
      response.error_message = base::StrCat(
          {"The ", pending->connector_id,
           " account is inactive - an expired trial or a suspension. Every "
           "request to it will fail until that is resolved."});
      std::move(pending->callback).Run(std::move(response));
      return;
    }
  }

  if (response.http_status == 401 || response.http_status == 403) {
    response.error = ConnectorError::kTokenExpired;
    response.error_message = base::StrCat(
        {pending->connector_id, " rejected the credentials (HTTP ",
         base::NumberToString(response.http_status),
         "). The connection may need renewing."});
    std::move(pending->callback).Run(std::move(response));
    return;
  }

  if (response.http_status < 200 || response.http_status >= 300) {
    response.error = ConnectorError::kHttpError;
    response.error_message =
        base::StrCat({pending->connector_id, " returned HTTP ",
                      base::NumberToString(response.http_status)});
    std::move(pending->callback).Run(std::move(response));
    return;
  }

  std::move(pending->callback).Run(std::move(response));
}

}  // namespace flux
