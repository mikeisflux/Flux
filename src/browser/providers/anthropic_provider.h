// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_ANTHROPIC_PROVIDER_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_ANTHROPIC_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/providers/llm_provider.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace flux {

// Claude, via the Anthropic Messages API.
//
// Default for agentic work: long tool-use chains are where it is strongest,
// and a browser agent is nothing but a long tool-use chain.
class AnthropicProvider : public LLMProvider {
 public:
  explicit AnthropicProvider(Profile* profile);
  ~AnthropicProvider() override;

  // LLMProvider:
  void Complete(CompletionRequest request, CompletionCallback callback) override;
  std::string GetProviderName() const override;
  double InputCostPerMillion(const std::string& model) const override;
  double OutputCostPerMillion(const std::string& model) const override;

 private:
  // Serializes a provider-neutral request into the Messages API wire format.
  // Notably tool_result blocks must be the FIRST content in a user turn, which
  // differs from OpenAI's separate "tool" role - the main structural
  // difference between the two providers.
  std::string BuildRequestBody(const CompletionRequest& request) const;

  void OnResponse(CompletionCallback callback,
                  std::unique_ptr<std::string> body);

  raw_ptr<Profile> profile_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<AnthropicProvider> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_PROVIDERS_ANTHROPIC_PROVIDER_H_
