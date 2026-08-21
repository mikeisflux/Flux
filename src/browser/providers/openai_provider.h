// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_PROVIDERS_OPENAI_PROVIDER_H_
#define CHROME_BROWSER_FLUX_PROVIDERS_OPENAI_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/providers/llm_provider.h"

class Profile;

namespace network {
class SimpleURLLoader;
}

namespace flux {

// OpenAI, via the Chat Completions API.
//
// Present so model choice is a runtime decision rather than a build-time one:
// cheap models for mechanical extraction, frontier models for judgment, and
// automatic failover when one provider is rate-limited mid-run.
class OpenAIProvider : public LLMProvider {
 public:
  explicit OpenAIProvider(Profile* profile);
  // Uses `key` instead of the stored credential. Used to validate a candidate
  // key before it is written to disk.
  OpenAIProvider(Profile* profile, std::string key);
  ~OpenAIProvider() override;

  // LLMProvider:
  void Complete(CompletionRequest request, CompletionCallback callback) override;
  std::string GetProviderName() const override;
  double InputCostPerMillion(const std::string& model) const override;
  double OutputCostPerMillion(const std::string& model) const override;

 private:
  // Chat Completions differs from Anthropic in two ways that matter here:
  // tool results are their own "tool" role message rather than blocks inside a
  // user turn, and tool arguments arrive as a JSON *string* that has to be
  // parsed rather than as a nested object.
  std::string BuildRequestBody(const CompletionRequest& request) const;

  void OnResponse(CompletionCallback callback,
                  std::unique_ptr<std::string> body);

  raw_ptr<Profile> profile_;
  // When set, overrides the stored key.
  std::string explicit_key_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<OpenAIProvider> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_PROVIDERS_OPENAI_PROVIDER_H_
