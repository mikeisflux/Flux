// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_LLM_PROVIDER_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_LLM_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"

namespace flux {

// A tool the model may call. Registered by ToolRegistry and serialized into
// whichever wire format the concrete provider needs.
struct ToolDefinition {
  std::string name;
  std::string description;
  base::DictValue input_schema;  // JSON Schema
};

struct ToolCall {
  std::string id;
  std::string name;
  base::DictValue input;

  // base::DictValue is move-only, which makes ToolCall move-only too. That is
  // worth keeping rather than working around: copying a tool call duplicates
  // its whole argument tree, and that should be visible at the call site.
  ToolCall Clone() const { return ToolCall{id, name, input.Clone()}; }
};

struct ToolResult {
  std::string tool_call_id;
  std::string content;
  bool is_error = false;
};

// One turn of conversation. Kept provider-neutral; each provider maps this
// onto its own message format.
struct Message {
  enum class Role { kUser, kAssistant, kSystem };
  Role role = Role::kUser;
  std::string text;
  std::vector<ToolCall> tool_calls;      // assistant turns
  std::vector<ToolResult> tool_results;  // user turns

  // Move-only for the same reason as ToolCall, which it contains.
  Message Clone() const {
    Message out;
    out.role = role;
    out.text = text;
    out.tool_results = tool_results;  // no DictValue; copyable
    out.tool_calls.reserve(tool_calls.size());
    for (const ToolCall& call : tool_calls) {
      out.tool_calls.push_back(call.Clone());
    }
    return out;
  }
};

// The conversation has to outlive each request built from it, so the history
// is cloned per turn rather than moved.
inline std::vector<Message> CloneMessages(const std::vector<Message>& messages) {
  std::vector<Message> out;
  out.reserve(messages.size());
  for (const Message& message : messages) {
    out.push_back(message.Clone());
  }
  return out;
}

struct CompletionRequest {
  std::string system_prompt;
  std::vector<Message> messages;
  std::vector<ToolDefinition> tools;
  std::string model;
  uint32_t max_output_tokens = 4096;
};

struct CompletionResponse {
  std::string text;
  std::vector<ToolCall> tool_calls;
  uint32_t input_tokens = 0;
  uint32_t output_tokens = 0;
  // True when the model stopped because it wants tool results back.
  bool wants_tools = false;
  // Set on failure. Retryable distinguishes rate limits and 5xx (worth a
  // backoff or a failover) from malformed requests (which will never succeed).
  std::string error;
  bool retryable = false;
};

// Abstract interface over Claude and OpenAI. The agent loop is written against
// this and knows nothing about either wire format, which is what makes
// per-task model routing and mid-run failover possible.
//
// The reference product exposes no model choice anywhere in its UI, implying a
// single fixed model. Flux treats the provider as a runtime decision: cheap
// models for mechanical extraction, frontier models for judgment.
class LLMProvider {
 public:
  using CompletionCallback = base::OnceCallback<void(CompletionResponse)>;

  virtual ~LLMProvider() = default;

  virtual void Complete(CompletionRequest request,
                        CompletionCallback callback) = 0;

  // Stable identifier used in run records and billing attribution.
  virtual std::string GetProviderName() const = 0;

  // Cost per million tokens, used to convert usage into credits before the
  // request is made so a budget can be enforced up front rather than after.
  virtual double InputCostPerMillion(const std::string& model) const = 0;
  virtual double OutputCostPerMillion(const std::string& model) const = 0;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_PROVIDERS_LLM_PROVIDER_H_
