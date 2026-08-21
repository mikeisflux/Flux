// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/webui/flux_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {

FluxPageHandler::FluxPageHandler(
    mojo::PendingReceiver<mojom::FluxPageHandler> receiver,
    mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      service_(FluxAgentServiceFactory::GetForProfile(profile)),
      receiver_(this, std::move(receiver)),
      observer_(std::move(observer)) {
  if (service_)
    observation_.Observe(service_.get());
}

FluxPageHandler::~FluxPageHandler() = default;

void FluxPageHandler::StartRun(mojom::TaskSpecPtr spec,
                               StartRunCallback callback) {
  if (!service_) {
    std::move(callback).Run(std::string(), "Agent service unavailable.");
    return;
  }
  std::string error;
  std::optional<std::string> run_id =
      service_->StartRun(std::move(spec), &error);
  if (!run_id) {
    std::move(callback).Run(std::string(), error);
    return;
  }
  std::move(callback).Run(*run_id, std::nullopt);
}

void FluxPageHandler::CancelRun(const std::string& run_id) {
  if (service_)
    service_->CancelRun(run_id);
}

void FluxPageHandler::PauseRun(const std::string& run_id) {
  if (service_)
    service_->PauseRun(run_id);
}

void FluxPageHandler::ResumeRun(const std::string& run_id) {
  if (service_)
    service_->ResumeRun(run_id);
}

void FluxPageHandler::ResolveApproval(
    const std::string& run_id,
    bool approved,
    const std::optional<std::string>& user_note) {
  if (service_)
    service_->ResolveApproval(run_id, approved, user_note.value_or(""));
}

void FluxPageHandler::ListRuns(ListRunsCallback callback) {
  std::move(callback).Run(service_ ? service_->ListRuns()
                                   : std::vector<mojom::RunProgressPtr>());
}

void FluxPageHandler::GetActions(const std::string& run_id,
                                 GetActionsCallback callback) {
  std::move(callback).Run(service_ ? service_->GetActions(run_id)
                                   : std::vector<mojom::ActionRecordPtr>());
}

void FluxPageHandler::CompileReplay(const std::string& run_id,
                                    CompileReplayCallback callback) {
  if (!service_) {
    std::move(callback).Run(std::nullopt, "Agent service unavailable.");
    return;
  }
  std::string error;
  std::optional<std::string> workflow_id =
      service_->CompileReplay(run_id, &error);
  std::move(callback).Run(workflow_id,
                          workflow_id ? std::nullopt
                                      : std::make_optional(error));
}

void FluxPageHandler::GetConcurrencyLimit(
    GetConcurrencyLimitCallback callback) {
  if (!service_) {
    std::move(callback).Run(0, 0, 0);
    return;
  }
  // Surfacing active and queued separately is what lets the console say
  // "4 running, 2 queued" rather than leaving a queued task looking stalled.
  FluxAgentService::Concurrency c = service_->GetConcurrency();
  std::move(callback).Run(c.limit, c.active, c.queued);
}

namespace {

// Cheapest possible request that still proves the credential is accepted:
// one token of output, no tools, trivial prompt.
CompletionRequest ProbeRequest(mojom::Provider provider) {
  CompletionRequest request;
  request.model = provider == mojom::Provider::kAnthropic ? "claude-haiku-4-5"
                                                          : "gpt-5-mini";
  request.max_output_tokens = 1;
  Message m;
  m.role = Message::Role::kUser;
  m.text = "hi";
  request.messages.push_back(std::move(m));
  return request;
}

std::string ProviderId(mojom::Provider provider) {
  return provider == mojom::Provider::kAnthropic ? "anthropic" : "openai";
}

// Shown to the user verbatim, so it has to say what to do rather than echo an
// HTTP status.
std::string Explain(const std::string& raw) {
  if (raw.find("401") != std::string::npos ||
      raw.find("authentication") != std::string::npos ||
      raw.find("invalid_api_key") != std::string::npos) {
    return "That key was rejected. Check it was copied in full and has not "
           "been revoked.";
  }
  if (raw.find("429") != std::string::npos ||
      raw.find("quota") != std::string::npos) {
    return "The key is valid but the account is out of quota or rate limited. "
           "Check the billing page for the provider.";
  }
  if (raw.find("Network error") != std::string::npos)
    return "Could not reach the provider. Check the network connection.";
  return raw;
}

}  // namespace

void FluxPageHandler::ProbeKey(
    mojom::Provider provider,
    const std::string& key,
    base::OnceCallback<void(bool, std::string)> done) {
  // Construct a provider bound to the candidate key rather than the stored
  // one, so an unsaved key can be checked before it is written anywhere.
  std::unique_ptr<LLMProvider> client;
  if (provider == mojom::Provider::kAnthropic)
    client = std::make_unique<AnthropicProvider>(profile_, key);
  else
    client = std::make_unique<OpenAIProvider>(profile_, key);

  LLMProvider* raw = client.get();
  raw->Complete(
      ProbeRequest(provider),
      base::BindOnce(
          [](std::unique_ptr<LLMProvider> owned,
             base::OnceCallback<void(bool, std::string)> cb,
             CompletionResponse response) {
            // A completed request with any content - or a max_tokens stop -
            // means the credential was accepted.
            std::move(cb).Run(response.error.empty(), Explain(response.error));
          },
          std::move(client), std::move(done)));
}

void FluxPageHandler::ListProviderKeys(ListProviderKeysCallback callback) {
  std::vector<mojom::ProviderKeyStatusPtr> out;
  for (mojom::Provider provider :
       {mojom::Provider::kAnthropic, mojom::Provider::kOpenAI}) {
    auto status = mojom::ProviderKeyStatus::New();
    status->provider = provider;
    const std::string key = GetApiKey(profile_, ProviderId(provider));
    status->configured = !key.empty();
    // Never return the key. Four characters is enough to tell two keys apart
    // and not enough to be worth anything.
    status->hint = key.size() >= 4 ? key.substr(key.size() - 4) : std::string();
    status->validated = validated_.count(ProviderId(provider)) > 0;
    out.push_back(std::move(status));
  }
  std::move(callback).Run(std::move(out));
}

void FluxPageHandler::SetProviderKey(mojom::Provider provider,
                                     const std::string& key,
                                     SetProviderKeyCallback callback) {
  if (key.empty()) {
    std::move(callback).Run(false, "No key entered.");
    return;
  }

  ProbeKey(provider, key,
           base::BindOnce(
               [](base::WeakPtr<FluxPageHandler> self, mojom::Provider provider,
                  std::string key, SetProviderKeyCallback cb, bool ok,
                  std::string error) {
                 if (!self) {
                   std::move(cb).Run(false, "Cancelled.");
                   return;
                 }
                 if (!ok) {
                   // Not stored. A key that does not work is worse than no key,
                   // because it turns into a failure mid-task later.
                   std::move(cb).Run(false, error);
                   return;
                 }
                 SetApiKey(self->profile_, ProviderId(provider), key);
                 self->validated_.insert(ProviderId(provider));
                 std::move(cb).Run(true, std::nullopt);
               },
               weak_factory_.GetWeakPtr(), provider, key, std::move(callback)));
}

void FluxPageHandler::ClearProviderKey(mojom::Provider provider) {
  SetApiKey(profile_, ProviderId(provider), std::string());
  validated_.erase(ProviderId(provider));
}

void FluxPageHandler::ValidateProviderKey(mojom::Provider provider,
                                          ValidateProviderKeyCallback callback) {
  const std::string key = GetApiKey(profile_, ProviderId(provider));
  if (key.empty()) {
    std::move(callback).Run(false, "No key configured for this provider.");
    return;
  }
  ProbeKey(provider, key,
           base::BindOnce(
               [](base::WeakPtr<FluxPageHandler> self, mojom::Provider provider,
                  ValidateProviderKeyCallback cb, bool ok, std::string error) {
                 if (self) {
                   if (ok)
                     self->validated_.insert(ProviderId(provider));
                   else
                     self->validated_.erase(ProviderId(provider));
                 }
                 std::move(cb).Run(ok, ok ? std::nullopt
                                          : std::make_optional(error));
               },
               weak_factory_.GetWeakPtr(), provider, std::move(callback)));
}

void FluxPageHandler::OnRunProgress(const mojom::RunProgress& progress) {
  observer_->OnRunProgress(progress.Clone());
}

void FluxPageHandler::OnRunAction(const std::string& run_id,
                                  const mojom::ActionRecord& action) {
  observer_->OnAction(run_id, action.Clone());
}

void FluxPageHandler::OnApprovalRequested(
    const mojom::ApprovalRequest& request) {
  observer_->OnApprovalRequested(request.Clone());
}

void FluxPageHandler::OnRunFinished(const std::string& run_id,
                                    mojom::RunState state,
                                    const std::string& summary) {
  observer_->OnRunFinished(run_id, state, summary);
}

}  // namespace flux
