// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_REGISTRY_H_
#define CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_REGISTRY_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chrome/browser/flux/mojom/flux.mojom.h"

namespace flux {

// How a connector authenticates. Mirrors auth.type in the definition files.
enum class AuthType {
  kOAuth2,
  kApiKey,
  kLocal,       // reads something on this machine; nothing to authorize
  kMcp,         // reached over MCP; no base URL of our own
  kUnsupported, // catalogued so the console can say why, not connectable
};

// One callable endpoint, straight out of the definition's `operations` map.
struct ConnectorOperation {
  std::string name;          // "create_todo"
  std::string label;         // "Create a to-do" - what a person calls it
  std::string method;        // "POST"
  std::string path;          // "/todolists/{todolist_id}/todos.json"
  mojom::WriteScope write_scope = mojom::WriteScope::kReadOnly;
  std::string notes;

  // Documented parameters, name -> what the definition says about it
  // ("1-500, default 50"). Query or path parameters in `params`, request-body
  // fields in `body`.
  //
  // These were authored for 35 operations and parsed by nothing, so the model
  // was told an operation's name, method and path and left to invent the rest
  // - while connector_list's own description promised it "operation names and
  // their required parameters are not guessable". It was right about that and
  // then did not supply them.
  std::map<std::string, std::string> params;
  std::map<std::string, std::string> body;

  // True when `path` is a full URL rather than a suffix of the base URL.
  // Basecamp's authorization document is the case that forced this: the call
  // that discovers the base URL cannot itself be relative to it.
  bool IsAbsolute() const;
};

// How to find the API base URL after authorizing, for the providers that do
// not have a fixed one.
//
// Declarative rather than a branch per connector: Basecamp is the only one in
// the catalogue that needs it today, but "GET a document, pick the entry that
// matches, take this field" is the shape every provider that has a discovery
// step uses, and a Basecamp-shaped `if` in the client would have to be joined
// by a Salesforce-shaped one the moment instance URLs come up.
struct AccountDiscovery {
  std::string endpoint;         // absolute URL, fetched with the new token
  std::string accounts_path;    // key of the list to search, e.g. "accounts"
  std::map<std::string, std::string> match;  // all pairs must match
  std::string base_url_field;   // field on the matched entry, e.g. "href"

  bool valid() const {
    return !endpoint.empty() && !accounts_path.empty() &&
           !base_url_field.empty();
  }
};

// A connector's auth block.
struct ConnectorAuth {
  AuthType type = AuthType::kUnsupported;
  std::string authorize_url;
  std::string token_url;
  // "basic" or "post". How the client credentials are presented at the token
  // endpoint; empty means the definition did not say, and post is assumed.
  std::string client_auth;
  std::vector<std::string> scopes;

  // A user-pasted token, where the definition offers one as an alternative to
  // the OAuth dance. Empty label means it does not.
  std::string personal_token_label;
  std::string personal_token_where;
  std::string personal_token_prefix;

  AccountDiscovery discovery;

  // An OAuth app Flux itself registered with the provider, so the user does
  // not have to.
  //
  // For a desktop application the "secret" is not one: the binary is
  // downloadable, the provider knows it, and Google's own installed-app flow
  // documents that it is not treated as confidential - which is why PKCE is
  // mandatory there and why every one of these definitions sets it. Shipping
  // one turns registering an OAuth app, enabling APIs and configuring a
  // consent screen into a single Connect button.
  //
  // Empty for every connector that has no such registration, and a
  // user-registered app always wins over it - see ConnectorService::GetClient.
  std::string builtin_client_id;
  std::string builtin_client_secret;

  bool has_personal_token() const { return !personal_token_label.empty(); }
  bool has_builtin_client() const { return !builtin_client_id.empty(); }
};

// One connector as the runtime sees it.
struct ConnectorDef {
  std::string id;
  ConnectorAuth auth;

  // May contain a {placeholder}: Basecamp's base URL is not known until the
  // authorization document names it. ResolveBaseUrl() below substitutes.
  std::string base_url;

  // Header template values may reference {access_token} and {api_token}.
  std::map<std::string, std::string> headers;

  std::map<std::string, ConnectorOperation> operations;

  // The prose worth telling the agent before it touches this service.
  std::vector<std::string> gotchas;

  const ConnectorOperation* FindOperation(const std::string& name) const;
};

// Loads the packed connector definitions.
//
// The definitions are authored as data/connectors/*.json and packed into
// flux_resources.pak by tools/build-connectors-json.py. Nothing reads the
// authoring files at runtime - they do not survive installation.
class ConnectorRegistry {
 public:
  ConnectorRegistry();
  ~ConnectorRegistry();

  ConnectorRegistry(const ConnectorRegistry&) = delete;
  ConnectorRegistry& operator=(const ConnectorRegistry&) = delete;

  const ConnectorDef* Get(const std::string& id) const;
  std::vector<const ConnectorDef*> All() const;
  size_t size() const { return connectors_.size(); }

  // Parses the packed JSON. Exposed for the sake of being testable without a
  // resource bundle; the constructor calls it with the packed resource.
  bool LoadFromJson(const std::string& json);

 private:
  std::map<std::string, ConnectorDef> connectors_;
};

// Substitutes {placeholders} in `url_template` from `values`. A placeholder
// with no value is left alone rather than replaced with an empty string,
// which would silently produce a wrong-but-valid path.
//
// It does NOT fail on its own, and a caller must not assume GURL will catch
// the leftover - see FirstUnresolvedPlaceholder.
std::string ResolveTemplate(const std::string& url_template,
                            const std::map<std::string, std::string>& values);

// The name inside the first {placeholder} still present in `url`, or nullopt.
//
// Necessary because an unresolved placeholder does not make a URL invalid.
// url/url_canon_path.cc marks '{' and '}' ESCAPE, not reject, so GURL
// percent-encodes them and reports the URL as perfectly valid - the request
// goes out to /projects/%7Bproject_id%7D.json and comes back a 404 that reads
// like the provider's fault. Checking is_valid() cannot detect this; only
// looking for the brace can.
std::optional<std::string> FirstUnresolvedPlaceholder(std::string_view url);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_CONNECTORS_CONNECTOR_REGISTRY_H_
