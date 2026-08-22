// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace flux {
namespace {

constexpr char kEndpoint[] = "https://api.anthropic.com/v1/messages";
constexpr char kApiVersion[] = "2023-06-01";

// Model pricing, USD per million tokens. Used to charge a run's budget before
// the request is dispatched rather than discovering an overrun afterwards.
struct Pricing { const char* model; double input; double output; };
constexpr Pricing kPricing[] = {
    {"claude-fable-5",   10.00, 50.00},
    {"claude-opus-5",     5.00, 25.00},
    {"claude-opus-4-8",   5.00, 25.00},
    {"claude-sonnet-5",   3.00, 15.00},
    {"claude-haiku-4-5",  1.00,  5.00},
};

double LookupPrice(const std::string& model, bool output) {
  for (const auto& p : kPricing) {
    if (model == p.model)
      return output ? p.output : p.input;
  }
  // Unknown model: assume the most expensive tier so a budget is never
  // silently under-charged.
  return output ? 50.00 : 10.00;
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
        // Custom delimiter: the annotation body contains `)"` inside
    // destination_other, which ends a plain R"( ... )" literal early.
    net::DefineNetworkTrafficAnnotation("flux_anthropic_completion", R"FLUX(
      semantics {
        sender: "Flux Agent"
        description:
          "Sends the current agent task, conversation history and a snapshot "
          "of the page the agent is working on to the Anthropic API to decide "
          "the next action to take."
        trigger:
          "The user starts an agent task, or a scheduled workflow fires."
        data:
          "The task prompt, prior agent actions and their results, and the "
          "accessible text of pages the agent has opened. This can include "
          "content from authenticated pages the agent was directed to."
        destination: OTHER
        destination_other: "Anthropic API (api.anthropic.com)"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Disabled unless the user configures an Anthropic API key and starts "
          "an agent task."
      })FLUX");

}  // namespace

AnthropicProvider::AnthropicProvider(Profile* profile) : profile_(profile) {}

AnthropicProvider::AnthropicProvider(Profile* profile, std::string key)
    : profile_(profile), explicit_key_(std::move(key)) {}
AnthropicProvider::~AnthropicProvider() = default;

std::string AnthropicProvider::GetProviderName() const { return "anthropic"; }

double AnthropicProvider::InputCostPerMillion(const std::string& m) const {
  return LookupPrice(m, /*output=*/false);
}
double AnthropicProvider::OutputCostPerMillion(const std::string& m) const {
  return LookupPrice(m, /*output=*/true);
}

std::string AnthropicProvider::BuildRequestBody(
    const CompletionRequest& request) const {
  base::DictValue root;
  root.Set("model", request.model);
  root.Set("max_tokens", static_cast<int>(request.max_output_tokens));
  if (!request.system_prompt.empty())
    root.Set("system", request.system_prompt);

  base::ListValue messages;
  for (const Message& m : request.messages) {
    base::DictValue msg;
    base::ListValue content;

    // Tool results must lead the user turn. Emitting them after text is
    // accepted by the API but degrades adherence noticeably.
    for (const ToolResult& tr : m.tool_results) {
      base::DictValue block;
      block.Set("type", "tool_result");
      block.Set("tool_use_id", tr.tool_call_id);
      block.Set("content", tr.content);
      if (tr.is_error)
        block.Set("is_error", true);
      content.Append(std::move(block));
    }

    if (!m.text.empty()) {
      base::DictValue block;
      block.Set("type", "text");
      block.Set("text", m.text);
      content.Append(std::move(block));
    }

    for (const ToolCall& tc : m.tool_calls) {
      base::DictValue block;
      block.Set("type", "tool_use");
      block.Set("id", tc.id);
      block.Set("name", tc.name);
      block.Set("input", tc.input.Clone());
      content.Append(std::move(block));
    }

    if (content.empty())
      continue;

    msg.Set("role", m.role == Message::Role::kAssistant ? "assistant" : "user");
    msg.Set("content", std::move(content));
    messages.Append(std::move(msg));
  }
  root.Set("messages", std::move(messages));

  if (!request.tools.empty()) {
    base::ListValue tools;
    for (const ToolDefinition& t : request.tools) {
      base::DictValue tool;
      tool.Set("name", t.name);
      tool.Set("description", t.description);
      tool.Set("input_schema", t.input_schema.Clone());
      tools.Append(std::move(tool));
    }
    root.Set("tools", std::move(tools));
  }

  std::string json;
  base::JSONWriter::Write(root, &json);
  return json;
}

void AnthropicProvider::Complete(CompletionRequest request,
                                 CompletionCallback callback) {
  const std::string key =
      explicit_key_.empty() ? GetApiKey(profile_, "anthropic") : explicit_key_;
  if (key.empty()) {
    CompletionResponse r;
    r.error = "No Anthropic API key configured. Add one in Settings > Agent.";
    r.retryable = false;
    std::move(callback).Run(std::move(r));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kEndpoint);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("x-api-key", key);
  resource_request->headers.SetHeader("anthropic-version", kApiVersion);
  resource_request->headers.SetHeader("content-type", "application/json");

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kTrafficAnnotation);
  loader_->AttachStringForUpload(BuildRequestBody(request), "application/json");
  // Agent turns with a large page snapshot are slow; the default 30s timeout
  // truncates legitimate responses.
  loader_->SetTimeoutDuration(base::Seconds(180));
  loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&AnthropicProvider::OnResponse, weak_factory_.GetWeakPtr(),
                     std::move(callback)),
      /*max_body_size=*/10 * 1024 * 1024);
}

void AnthropicProvider::OnResponse(CompletionCallback callback,
                                   std::optional<std::string> body) {
  CompletionResponse result;

  const int http_status =
      loader_->ResponseInfo() && loader_->ResponseInfo()->headers
          ? loader_->ResponseInfo()->headers->response_code()
          : 0;

  if (!body) {
    result.error = base::StrCat({"Network error contacting Anthropic (",
                                 net::ErrorToShortString(loader_->NetError()),
                                 ")"});
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  // 429 and 5xx are worth a backoff or a failover to the other provider;
  // 400 and 401 will fail identically forever.
  if (http_status == 429 || http_status >= 500) {
    result.error = base::StrCat({"Anthropic returned ",
                                 base::NumberToString(http_status)});
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  std::optional<base::Value> parsed = base::JSONReader::Read(*body, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    result.error = "Malformed response from Anthropic.";
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }
  const base::DictValue& root = parsed->GetDict();

  if (const base::DictValue* error = root.FindDict("error")) {
    const std::string* message = error->FindString("message");
    result.error = message ? *message : "Unknown Anthropic error.";
    result.retryable = false;
    std::move(callback).Run(std::move(result));
    return;
  }

  if (const base::DictValue* usage = root.FindDict("usage")) {
    result.input_tokens = usage->FindInt("input_tokens").value_or(0);
    result.output_tokens = usage->FindInt("output_tokens").value_or(0);
  }

  if (const base::ListValue* content = root.FindList("content")) {
    for (const base::Value& block_value : *content) {
      const base::DictValue* block = block_value.GetIfDict();
      if (!block)
        continue;
      const std::string* type = block->FindString("type");
      if (!type)
        continue;

      if (*type == "text") {
        if (const std::string* text = block->FindString("text"))
          result.text += *text;
      } else if (*type == "tool_use") {
        ToolCall call;
        if (const std::string* id = block->FindString("id"))
          call.id = *id;
        if (const std::string* name = block->FindString("name"))
          call.name = *name;
        if (const base::DictValue* input = block->FindDict("input"))
          call.input = input->Clone();
        result.tool_calls.push_back(std::move(call));
      }
    }
  }

  const std::string* stop_reason = root.FindString("stop_reason");
  result.wants_tools = stop_reason && *stop_reason == "tool_use";

  std::move(callback).Run(std::move(result));
}

}  // namespace flux
