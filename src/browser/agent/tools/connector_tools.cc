// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/tools/connector_tools.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/connectors/connector_client.h"
#include "chrome/browser/flux/connectors/connector_registry.h"
#include "chrome/browser/flux/connectors/connector_service.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace flux {
namespace {

// A connector response can be megabytes. The client caps the download at 8MB,
// which is the right cap for the network but catastrophic for a context
// window - one unpaginated list would evict the entire task. Truncated here,
// visibly, so the model knows to narrow the request rather than assuming it
// saw everything.
constexpr size_t kMaxBodyChars = 20000;

ToolResult Ok(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  return r;
}

ToolResult Err(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  r.is_error = true;
  return r;
}

const char* ScopeName(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return "readonly";
    case mojom::WriteScope::kDraft:    return "draft";
    case mojom::WriteScope::kSend:     return "send";
    case mojom::WriteScope::kPurchase: return "purchase";
  }
  return "unknown";
}

base::DictValue StringProp(const std::string& description) {
  base::DictValue d;
  d.Set("type", "string");
  d.Set("description", description);
  return d;
}

base::DictValue ObjectProp(const std::string& description) {
  base::DictValue d;
  d.Set("type", "object");
  d.Set("description", description);
  return d;
}

base::DictValue ObjectSchema(base::DictValue properties,
                             std::vector<std::string> required) {
  base::DictValue schema;
  schema.Set("type", "object");
  schema.Set("properties", std::move(properties));
  base::ListValue req;
  for (const std::string& r : required)
    req.Append(r);
  schema.Set("required", std::move(req));
  return schema;
}

ConnectorService* ServiceFor(const ToolContext& ctx) {
  if (!ctx.web_contents)
    return nullptr;
  Profile* profile =
      Profile::FromBrowserContext(ctx.web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;
  FluxAgentService* service = FluxAgentServiceFactory::GetForProfile(profile);
  return service ? service->connectors() : nullptr;
}

// Flattens a JSON object of scalars into string pairs. Path parameters and
// query values are always strings on the wire, and a model that supplies
// {"page": 2} should not have that silently dropped for not being a string.
std::map<std::string, std::string> FlattenToStrings(
    const base::DictValue* dict) {
  std::map<std::string, std::string> out;
  if (!dict)
    return out;
  for (const auto [key, value] : *dict) {
    if (value.is_string()) {
      out[key] = value.GetString();
    } else if (value.is_int()) {
      out[key] = base::NumberToString(value.GetInt());
    } else if (value.is_bool()) {
      out[key] = value.GetBool() ? "true" : "false";
    } else if (value.is_double()) {
      out[key] = base::NumberToString(value.GetDouble());
    }
  }
  return out;
}

// --------------------------------------------------------------------------
// list_connectors

class ListConnectorsTool : public Tool {
 public:
  std::string name() const override { return "list_connectors"; }

  std::string description() const override {
    return "List the services this profile has connected, and what each one "
           "can do. Call with no arguments for the connected services; call "
           "with a connector id for that service's operations, the write "
           "scope of each, and the things that are known to go wrong with it. "
           "Always do this before using a connector you have not used in this "
           "task - operation names and their required parameters are not "
           "guessable.";
  }

  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }

  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("connector", StringProp(
        "Optional. A connector id, to list that service's operations."));
    return ObjectSchema(std::move(props), {});
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    return "List connected services";
  }

  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    ConnectorService* service = ServiceFor(ctx);
    if (!service) {
      std::move(callback).Run(Err("Connectors are unavailable in this run."));
      return;
    }

    const std::string* id = input.FindString("connector");
    if (!id) {
      std::move(callback).Run(Ok(Summary(service)));
      return;
    }

    const ConnectorDef* def = service->registry().Get(*id);
    if (!def) {
      std::move(callback).Run(
          Err(base::StrCat({"No connector called '", *id,
                            "'. Call list_connectors with no arguments to "
                            "see what exists."})));
      return;
    }
    std::move(callback).Run(Ok(Detail(service, *def)));
  }

 private:
  static std::string Summary(ConnectorService* service) {
    std::string out;
    for (const ConnectorStatus& status : service->ListStatus()) {
      if (!status.connected)
        continue;
      base::StrAppend(&out, {status.id});
      if (status.expired)
        base::StrAppend(&out, {"  (access expired - it will be renewed on the "
                               "next call)"});
      base::StrAppend(&out, {"\n"});
    }
    if (out.empty()) {
      return "No services are connected. Anything else has to be done by "
             "driving the site in the browser, which works for every site.";
    }
    return base::StrCat({"Connected services:\n", out});
  }

  static std::string Detail(ConnectorService* service,
                            const ConnectorDef& def) {
    const ConnectorStatus status = service->GetStatus(def.id);
    std::string out = base::StrCat({def.id, "\n"});
    if (!status.connected) {
      base::StrAppend(&out, {"NOT CONNECTED. ",
                             status.detail.empty()
                                 ? "Connect it on the connectors screen."
                                 : status.detail,
                             "\n"});
    }

    base::StrAppend(&out, {"\nOperations (name, method, path, scope):\n"});
    for (const auto& [name, op] : def.operations) {
      base::StrAppend(&out, {"  ", name, "  ", op.method, " ", op.path,
                             "  [", ScopeName(op.write_scope), "]\n"});
      if (!op.notes.empty())
        base::StrAppend(&out, {"      ", op.notes, "\n"});
    }

    // The gotchas are the reason this tool exists rather than the model
    // guessing REST from the operation name. "A message posted without a
    // status field is a draft nobody sees" is not inferable.
    if (!def.gotchas.empty()) {
      base::StrAppend(&out, {"\nKnown traps with this service:\n"});
      for (const std::string& gotcha : def.gotchas)
        base::StrAppend(&out, {"  - ", gotcha, "\n"});
    }
    return out;
  }
};

// --------------------------------------------------------------------------
// connector_read / connector_draft / connector_send

class ConnectorCallTool : public Tool {
 public:
  ConnectorCallTool(std::string name, mojom::WriteScope tier)
      : name_(std::move(name)), tier_(tier) {}

  std::string name() const override { return name_; }

  std::string description() const override {
    const char* tier = ScopeName(tier_);
    return base::StrCat({
        "Call a ", tier, " operation on a connected service. Use "
        "list_connectors with a connector id first to get the operation name "
        "and what it needs. This tool can only run operations at ", tier,
        " scope or below; anything stronger is refused before the request is "
        "built."});
  }

  mojom::WriteScope RequiredScope() const override { return tier_; }

  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("connector", StringProp("Connector id, e.g. \"basecamp\"."));
    props.Set("operation", StringProp(
        "Operation name from list_connectors, e.g. \"list_projects\"."));
    props.Set("path_params", ObjectProp(
        "Values for the {placeholders} in the operation's path."));
    props.Set("query", ObjectProp("Query string parameters."));
    props.Set("body", ObjectProp(
        "Request body, as an object. Omit for a GET."));
    props.Set("follow_page", StringProp(
        "A next-page URL returned by a previous call. When set, that page is "
        "fetched and path_params, query and body are ignored."));
    return ObjectSchema(std::move(props), {"connector", "operation"});
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* connector = input.FindString("connector");
    const std::string* operation = input.FindString("operation");
    return base::StrCat({ScopeName(tier_), ": ",
                         connector ? *connector : "(no connector)", ".",
                         operation ? *operation : "(no operation)"});
  }

  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    ConnectorService* service = ServiceFor(ctx);
    if (!service) {
      std::move(callback).Run(Err("Connectors are unavailable in this run."));
      return;
    }

    const std::string* connector = input.FindString("connector");
    const std::string* operation = input.FindString("operation");
    if (!connector || !operation) {
      std::move(callback).Run(
          Err("Both 'connector' and 'operation' are required."));
      return;
    }

    // A next-page URL is already a complete URL from the provider; following
    // it is a fetch, not a rebuilt operation.
    if (const std::string* page = input.FindString("follow_page")) {
      if (!page->empty()) {
        service->FollowPage(*connector, *page, tier_,
                            base::BindOnce(&ConnectorCallTool::OnResponse,
                                           std::move(callback)));
        return;
      }
    }

    ConnectorClient::Request request;
    request.connector_id = *connector;
    request.operation = *operation;
    request.path_params = FlattenToStrings(input.FindDict("path_params"));
    request.query = FlattenToStrings(input.FindDict("query"));
    // This tool's own tier, NOT ctx.scope. The registry has already decided
    // this tool may be offered at all; passing the task's scope here would let
    // connector_read reach a send operation inside a send-scoped task.
    request.granted_scope = tier_;

    if (const base::DictValue* body = input.FindDict("body")) {
      std::optional<std::string> json = base::WriteJson(*body);
      if (!json) {
        std::move(callback).Run(Err("The body could not be serialized."));
        return;
      }
      request.body = *json;
    }

    service->Execute(std::move(request),
                     base::BindOnce(&ConnectorCallTool::OnResponse,
                                    std::move(callback)));
  }

 private:
  static void OnResponse(ResultCallback callback, ConnectorResponse response) {
    if (!response.ok()) {
      std::string message = response.error_message;
      if (response.error == ConnectorError::kRateLimited &&
          !response.retry_after.is_zero()) {
        base::StrAppend(&message,
                        {" Retry after ",
                         base::NumberToString(response.retry_after.InSeconds()),
                         " seconds."});
      }
      // The provider's own body is usually the only thing that says WHY, so it
      // rides along even on failure.
      if (!response.body.empty()) {
        base::StrAppend(&message, {"\n", Truncate(response.body)});
      }
      std::move(callback).Run(Err(std::move(message)));
      return;
    }

    std::string out = base::StrCat(
        {"HTTP ", base::NumberToString(response.http_status), "\n",
         Truncate(response.body)});
    if (!response.next_page_url.empty()) {
      base::StrAppend(
          &out, {"\n\nThere is another page. Call this operation again with "
                 "follow_page set to:\n", response.next_page_url});
    }
    std::move(callback).Run(Ok(std::move(out)));
  }

  static std::string Truncate(const std::string& body) {
    if (body.size() <= kMaxBodyChars)
      return body;
    return base::StrCat(
        {body.substr(0, kMaxBodyChars), "\n\n[truncated at ",
         base::NumberToString(kMaxBodyChars), " of ",
         base::NumberToString(body.size()),
         " characters. Narrow the request - a filter or a smaller page - "
         "rather than assuming this is the whole response.]"});
  }

  const std::string name_;
  const mojom::WriteScope tier_;
};

}  // namespace

void RegisterConnectorTools(ToolRegistry* registry) {
  registry->Register(std::make_unique<ListConnectorsTool>());
  registry->Register(std::make_unique<ConnectorCallTool>(
      "connector_read", mojom::WriteScope::kReadOnly));
  registry->Register(std::make_unique<ConnectorCallTool>(
      "connector_draft", mojom::WriteScope::kDraft));
  registry->Register(std::make_unique<ConnectorCallTool>(
      "connector_send", mojom::WriteScope::kSend));
}

}  // namespace flux
