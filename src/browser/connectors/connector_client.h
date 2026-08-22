// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_CLIENT_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/connectors/connector_registry.h"
#include "chrome/browser/flux/connectors/connector_tokens.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace flux {

// Why a request did not happen, or did not work. Separated from a plain error
// string because the agent has to act differently on each: a scope refusal is
// final, a rate limit is a wait, and an inactive account means stop asking.
enum class ConnectorError {
  kNone,
  kUnknownConnector,
  kUnknownOperation,
  kNotConnected,
  kScopeRefused,     // the operation writes more than the task is allowed to
  kTokenExpired,     // refresh failed; the user has to reconnect
  kRateLimited,      // retry_after is set
  kAccountInactive,  // the whole account is suspended. Do not retry, ever.
  kHttpError,
  kNetworkError,
};

struct ConnectorResponse {
  ConnectorError error = ConnectorError::kNone;
  std::string error_message;

  int http_status = 0;
  std::string body;

  // From a 429's Retry-After. Zero when the response did not carry one.
  base::TimeDelta retry_after;

  // The rel="next" URL out of an RFC 5988 Link header, empty on the last page.
  // Surfaced rather than followed: how many pages a task should walk is the
  // task's decision, and walking them all by default is how a connector burns
  // a rate limit on a project with nine thousand to-dos.
  std::string next_page_url;

  bool ok() const { return error == ConnectorError::kNone; }
};

// Turns an operation in a connector definition into an authorized HTTP
// request, and refuses the ones the task is not allowed to make.
//
// The write-scope gate lives here rather than in the agent loop on purpose.
// The reference product stated write-safety in prose and enforced nothing;
// putting the check at the one place every connector request passes through
// means a task cannot send by taking a different route to the same endpoint.
class ConnectorClient {
 public:
  using ResponseCallback = base::OnceCallback<void(ConnectorResponse)>;

  ConnectorClient(Profile* profile,
                  const ConnectorRegistry* registry,
                  ConnectorTokenStore* tokens);
  ~ConnectorClient();

  ConnectorClient(const ConnectorClient&) = delete;
  ConnectorClient& operator=(const ConnectorClient&) = delete;

  struct Request {
    std::string connector_id;
    std::string operation;

    // Substituted into the operation's path: {todolist_id} and friends.
    std::map<std::string, std::string> path_params;

    // Appended to the query string.
    std::map<std::string, std::string> query;

    // Request body, already serialized. Empty for a GET.
    std::string body;

    // What the running task is permitted to do. An operation whose write_scope
    // exceeds this is refused before anything is sent.
    mojom::WriteScope granted_scope = mojom::WriteScope::kReadOnly;
  };

  void Execute(Request request, ResponseCallback callback);

  // Fetches an absolute URL under a connector's credentials. This is how a
  // next_page_url gets followed: it is already a complete URL from the
  // provider, and re-deriving it from an operation path would be guesswork.
  void Follow(const std::string& connector_id,
              const std::string& url,
              mojom::WriteScope granted_scope,
              ResponseCallback callback);

  // True when `operation` is within `granted`. Exposed so the console can grey
  // out what a task could not do rather than offering it and failing.
  static bool ScopePermits(mojom::WriteScope granted,
                           mojom::WriteScope required);

 private:
  struct PendingRequest;

  void SendNow(std::unique_ptr<PendingRequest> pending);
  void OnResponse(std::unique_ptr<PendingRequest> pending,
                  std::optional<std::string> body);

  raw_ptr<Profile> profile_;
  raw_ptr<const ConnectorRegistry> registry_;
  raw_ptr<ConnectorTokenStore> tokens_;
  base::WeakPtrFactory<ConnectorClient> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_CLIENT_H_
