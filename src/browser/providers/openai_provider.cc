// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/providers/openai_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace flux {
namespace {

constexpr char kEndpoint[] = "https://api.openai.com/v1/chat/completions";

// USD per million tokens. Kept alongside the Anthropic table so a run's budget
// can be charged before dispatch regardless of which provider serves it.
struct Pricing { const char* model; double input; double output; };
constexpr Pricing kPricing[] = {
    {"gpt-5",       1.25, 10.00},
    {"gpt-5-mini",  0.25,  2.00},
    {"gpt-4.1",     2.00,  8.00},
    {"o3",          2.00,  8.00},
};

double LookupPrice(const std::string& model, bool output) {
  for (const auto& p : kPricing) {
    if (model == p.model)
      return output ? p.output : p.input;
  }
  // Unknown model: assume the priciest tier so a budget is never under-charged.
  return output ? 10.00 : 2.00;
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("flux_openai_completion", R"(
      semantics {
        sender: "Flux Agent"
        description:
          "Sends the current agent task, conversation history and a snapshot "
          "of the page the agent is working on to the OpenAI API to decide "
          "the next action to take."
        trigger:
          "The user starts an agent task configured to use OpenAI, or a "
          "scheduled workflow fires, or an Anthropic request fails over."
        data:
          "The task prompt, prior agent actions and their results, and the "
          "accessible text of pages the agent has opened. This can include "
          "content from authenticated pages the agent was directed to."
        destination: OTHER
        destination_other: "OpenAI API (api.openai.com)"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Disabled unless the user configures an OpenAI API key and starts an "
          "agent task."
      })");

}  // namespace

OpenAIProvider::OpenAIProvider(Profile* profile) : profile_(profile) {}
OpenAIProvider::~OpenAIProvider() = default;

std::string OpenAIProvider::GetProviderName() const { return "openai"; }

double OpenAIProvider::InputCostPerMillion(const std::string& m) const {
  return LookupPrice(m, /*output=*/false);
}
double OpenAIProvider::OutputCostPerMillion(const std::string& m) const {
  return LookupPrice(m, /*output=*/true);
}

std::string OpenAIProvider::BuildRequestBody(
    const CompletionRequest& request) const {
  base::Value::Dict root;
  root.Set("model", request.model);
  root.Set("max_completion_tokens",
           static_cast<int>(request.max_output_tokens));

  base::Value::List messages;
  if (!request.system_prompt.empty()) {
    base::Value::Dict sys;
    sys.Set("role", "system");
    sys.Set("content", request.system_prompt);
    messages.Append(std::move(sys));
  }

  for (const Message& m : request.messages) {
    // Tool results are separate top-level messages here, and each must
    // reference the call it answers.
    for (const ToolResult& tr : m.tool_results) {
      base::Value::Dict msg;
      msg.Set("role", "tool");
      msg.Set("tool_call_id", tr.tool_call_id);
      msg.Set("content", tr.content);
      messages.Append(std::move(msg));
    }

    if (m.text.empty() && m.tool_calls.empty())
      continue;

    base::Value::Dict msg;
    msg.Set("role",
            m.role == Message::Role::kAssistant ? "assistant" : "user");
    msg.Set("content", m.text);

    if (!m.tool_calls.empty()) {
      base::Value::List calls;
      for (const ToolCall& tc : m.tool_calls) {
        base::Value::Dict call;
        call.Set("id", tc.id);
        call.Set("type", "function");
        base::Value::Dict fn;
        fn.Set("name", tc.name);
        // Arguments go over the wire as a JSON string, not an object.
        std::string args;
        base::JSONWriter::Write(tc.input, &args);
        fn.Set("arguments", args);
        call.Set("function", std::move(fn));
        calls.Append(std::move(call));
      }
      msg.Set("tool_calls", std::move(calls));
    }
    messages.Append(std::move(msg));
  }
  root.Set("messages", std::move(messages));

  if (!request.tools.empty()) {
    base::Value::List tools;
    for (const ToolDefinition& t : request.tools) {
      base::Value::Dict tool;
      tool.Set("type", "function");
      base::Value::Dict fn;
      fn.Set("name", t.name);
      fn.Set("description", t.description);
      fn.Set("parameters", t.input_schema.Clone());
      tool.Set("function", std::move(fn));
      tools.Append(std::move(tool));
    }
    root.Set("tools", std::move(tools));
  }

  std::string json;
  base::JSONWriter::Write(root, &json);
  return json;
}

void OpenAIProvider::Complete(CompletionRequest request,
                              CompletionCallback callback) {
  const std::string key = GetApiKey(profile_, "openai");
  if (key.empty()) {
    CompletionResponse r;
    r.error = "No OpenAI API key configured. Add one in Settings > Agent.";
    r.retryable = false;
    std::move(callback).Run(std::move(r));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kEndpoint);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("Authorization",
                                      base::StrCat({"Bearer ", key}));
  resource_request->headers.SetHeader("content-type", "application/json");

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kTrafficAnnotation);
  loader_->AttachStringForUpload(BuildRequestBody(request), "application/json");
  loader_->SetTimeoutDuration(base::Seconds(180));
  loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&OpenAIProvider::OnResponse, weak_factory_.GetWeakPtr(),
                     std::move(callback)),
      /*max_body_size=*/10 * 1024 * 1024);
}

void OpenAIProvider::OnResponse(CompletionCallback callback,
                                std::unique_ptr<std::string> body) {
  CompletionResponse result;

  const int http_status =
      loader_->ResponseInfo() && loader_->ResponseInfo()->headers
          ? loader_->ResponseInfo()->headers->response_code()
          : 0;

  if (!body) {
    result.error = base::StrCat({"Network error contacting OpenAI (",
                                 net::ErrorToShortString(loader_->NetError()),
                                 ")"});
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  if (http_status == 429 || http_status >= 500) {
    result.error =
        base::StrCat({"OpenAI returned ", base::NumberToString(http_status)});
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  std::optional<base::Value> parsed = base::JSONReader::Read(*body);
  if (!parsed || !parsed->is_dict()) {
    result.error = "Malformed response from OpenAI.";
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }
  const base::Value::Dict& root = parsed->GetDict();

  if (const base::Value::Dict* error = root.FindDict("error")) {
    const std::string* message = error->FindString("message");
    result.error = message ? *message : "Unknown OpenAI error.";
    result.retryable = false;
    std::move(callback).Run(std::move(result));
    return;
  }

  if (const base::Value::Dict* usage = root.FindDict("usage")) {
    result.input_tokens = usage->FindInt("prompt_tokens").value_or(0);
    result.output_tokens = usage->FindInt("completion_tokens").value_or(0);
  }

  const base::Value::List* choices = root.FindList("choices");
  if (!choices || choices->empty()) {
    result.error = "OpenAI returned no choices.";
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  const base::Value::Dict* choice = choices->front().GetIfDict();
  const base::Value::Dict* message =
      choice ? choice->FindDict("message") : nullptr;
  if (!message) {
    result.error = "OpenAI response missing message.";
    result.retryable = true;
    std::move(callback).Run(std::move(result));
    return;
  }

  if (const std::string* content = message->FindString("content"))
    result.text = *content;

  if (const base::Value::List* calls = message->FindList("tool_calls")) {
    for (const base::Value& call_value : *calls) {
      const base::Value::Dict* call = call_value.GetIfDict();
      if (!call)
        continue;
      const base::Value::Dict* fn = call->FindDict("function");
      if (!fn)
        continue;

      ToolCall tc;
      if (const std::string* id = call->FindString("id"))
        tc.id = *id;
      if (const std::string* name = fn->FindString("name"))
        tc.name = *name;
      // Arguments arrive as a JSON string; a model can emit malformed JSON
      // here, so a parse failure is a tool error rather than a crash.
      if (const std::string* args = fn->FindString("arguments")) {
        std::optional<base::Value> parsed_args = base::JSONReader::Read(*args);
        if (parsed_args && parsed_args->is_dict())
          tc.input = parsed_args->GetDict().Clone();
      }
      result.tool_calls.push_back(std::move(tc));
    }
  }

  const std::string* finish = choice->FindString("finish_reason");
  result.wants_tools = finish && *finish == "tool_calls";

  std::move(callback).Run(std::move(result));
}

}  // namespace flux
