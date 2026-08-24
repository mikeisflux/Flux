// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/connectors/connector_registry.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/grit/flux_resources_map.h"
#include "ui/base/resource/resource_bundle.h"

namespace flux {
namespace {

// The path the packed resource is registered under, which is just its
// filename in src/resources.
constexpr char kResourcePath[] = "connector_defs.json";

AuthType ParseAuthType(const std::string* value) {
  if (!value)
    return AuthType::kUnsupported;
  if (*value == "oauth2")  return AuthType::kOAuth2;
  if (*value == "api_key") return AuthType::kApiKey;
  if (*value == "local")   return AuthType::kLocal;
  if (*value == "mcp")     return AuthType::kMcp;
  return AuthType::kUnsupported;
}

mojom::WriteScope ParseScope(const std::string* value) {
  if (!value)
    return mojom::WriteScope::kReadOnly;
  if (*value == "draft")    return mojom::WriteScope::kDraft;
  if (*value == "send")     return mojom::WriteScope::kSend;
  if (*value == "purchase") return mojom::WriteScope::kPurchase;
  return mojom::WriteScope::kReadOnly;
}

std::string StringOr(const base::DictValue& dict, std::string_view key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

// Copies an operation's documented parameters out of the definition.
//
// The values are prose written for a reader ("plainText (the only value)"),
// not a schema, so they are carried across as written. A non-string value is
// rendered rather than dropped: four definitions nest a dict here and one uses
// a bool, and a parameter that silently vanishes is the failure this whole
// field exists to prevent.
void ReadParams(const base::DictValue& op,
                std::string_view key,
                std::map<std::string, std::string>* out) {
  const base::DictValue* params = op.FindDict(key);
  if (!params)
    return;
  for (const auto [name, value] : *params) {
    if (const std::string* text = value.GetIfString()) {
      (*out)[name] = *text;
    } else if (std::optional<bool> flag = value.GetIfBool()) {
      (*out)[name] = *flag ? "true" : "false";
    } else if (std::optional<std::string> json = base::WriteJson(value)) {
      (*out)[name] = *json;
    }
  }
}

// Looks the packed resource up by path rather than by a generated IDR symbol.
//
// The symbol would be IDR_FLUX_CONNECTOR_DEFS_JSON, derived from the filename
// by grit. Getting that derivation wrong is a compile error found only inside
// a Chromium build - two hours - whereas a path miss here is an empty registry
// and a log line. The path is also the thing BUILD.gn actually states.
int ResourceIdForPath(std::string_view path) {
  for (const webui::ResourcePath& entry : kFluxResources) {
    if (path == entry.path)
      return entry.id;
  }
  return -1;
}

}  // namespace

bool ConnectorOperation::IsAbsolute() const {
  return base::StartsWith(path, "https://") ||
         base::StartsWith(path, "http://");
}

const ConnectorOperation* ConnectorDef::FindOperation(
    const std::string& name) const {
  auto it = operations.find(name);
  return it == operations.end() ? nullptr : &it->second;
}

ConnectorRegistry::ConnectorRegistry() {
  const int id = ResourceIdForPath(kResourcePath);
  if (id < 0) {
    LOG(ERROR) << "flux: " << kResourcePath << " is not a packed resource - "
               << "no connector can be used. Is it in src/resources/BUILD.gn?";
    return;
  }
  if (!LoadFromJson(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(id))) {
    LOG(ERROR) << "flux: " << kResourcePath << " did not parse.";
  }
}

ConnectorRegistry::~ConnectorRegistry() = default;

bool ConnectorRegistry::LoadFromJson(const std::string& json) {
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed)
    return false;

  const base::ListValue* list = parsed->FindList("connectors");
  if (!list)
    return false;

  for (const base::Value& entry : *list) {
    if (!entry.is_dict())
      continue;
    const base::DictValue& dict = entry.GetDict();

    ConnectorDef def;
    def.id = StringOr(dict, "id");
    if (def.id.empty())
      continue;

    if (const base::DictValue* auth = dict.FindDict("auth")) {
      def.auth.type = ParseAuthType(auth->FindString("type"));
      def.auth.authorize_url = StringOr(*auth, "authorize_url");
      def.auth.token_url = StringOr(*auth, "token_url");
      def.auth.client_auth = StringOr(*auth, "client_auth");
      if (const base::ListValue* scopes = auth->FindList("scopes")) {
        for (const base::Value& scope : *scopes) {
          if (scope.is_string())
            def.auth.scopes.push_back(scope.GetString());
        }
      }
      if (const base::DictValue* ad = auth->FindDict("account_discovery")) {
        def.auth.discovery.endpoint = StringOr(*ad, "endpoint");
        def.auth.discovery.accounts_path = StringOr(*ad, "accounts_path");
        def.auth.discovery.base_url_field = StringOr(*ad, "base_url_field");
        if (const base::DictValue* match = ad->FindDict("match")) {
          for (const auto [key, value] : *match) {
            if (value.is_string())
              def.auth.discovery.match[key] = value.GetString();
          }
        }
      }
      if (const base::DictValue* client = auth->FindDict("client")) {
        def.auth.builtin_client_id = StringOr(*client, "id");
        def.auth.builtin_client_secret = StringOr(*client, "secret");
      }
      if (const base::DictValue* pt = auth->FindDict("personal_token")) {
        def.auth.personal_token_label = StringOr(*pt, "label");
        def.auth.personal_token_where = StringOr(*pt, "where");
        def.auth.personal_token_prefix = StringOr(*pt, "prefix");
      }
    }

    if (const base::DictValue* api = dict.FindDict("api")) {
      def.base_url = StringOr(*api, "base_url");
      if (const base::DictValue* headers = api->FindDict("headers")) {
        for (const auto [name, value] : *headers) {
          if (value.is_string())
            def.headers[name] = value.GetString();
        }
      }
    }

    if (const base::DictValue* ops = dict.FindDict("operations")) {
      for (const auto [name, value] : *ops) {
        if (!value.is_dict())
          continue;
        const base::DictValue& op = value.GetDict();
        ConnectorOperation parsed_op;
        parsed_op.name = name;
        parsed_op.label = StringOr(op, "label");
        parsed_op.method = StringOr(op, "method");
        parsed_op.path = StringOr(op, "path");
        parsed_op.write_scope = ParseScope(op.FindString("write_scope"));
        parsed_op.notes = StringOr(op, "notes");
        ReadParams(op, "params", &parsed_op.params);
        ReadParams(op, "body", &parsed_op.body);
        def.operations[name] = std::move(parsed_op);
      }
    }

    if (const base::ListValue* gotchas = dict.FindList("gotchas")) {
      for (const base::Value& gotcha : *gotchas) {
        if (gotcha.is_string())
          def.gotchas.push_back(gotcha.GetString());
      }
    }

    connectors_[def.id] = std::move(def);
  }
  return true;
}

const ConnectorDef* ConnectorRegistry::Get(const std::string& id) const {
  auto it = connectors_.find(id);
  return it == connectors_.end() ? nullptr : &it->second;
}

std::vector<const ConnectorDef*> ConnectorRegistry::All() const {
  std::vector<const ConnectorDef*> out;
  out.reserve(connectors_.size());
  for (const auto& [id, def] : connectors_)
    out.push_back(&def);
  return out;
}

std::string ResolveTemplate(const std::string& url_template,
                            const std::map<std::string, std::string>& values) {
  std::string out = url_template;
  for (const auto& [key, value] : values) {
    base::ReplaceSubstringsAfterOffset(&out, 0, base::StrCat({"{", key, "}"}),
                                       value);
  }
  return out;
}

std::optional<std::string> FirstUnresolvedPlaceholder(std::string_view url) {
  const size_t open = url.find('{');
  if (open == std::string_view::npos)
    return std::nullopt;
  const size_t close = url.find('}', open + 1);
  if (close == std::string_view::npos)
    return std::nullopt;
  return std::string(url.substr(open + 1, close - open - 1));
}

}  // namespace flux
