// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/webui/flux_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/flux/connectors/connector_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flux {

FluxPageHandler::FluxPageHandler(
    mojo::PendingReceiver<mojom::FluxPageHandler> receiver,
    mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      service_(FluxAgentServiceFactory::GetForProfile(profile)),
      web_contents_(web_contents),
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

// --- Customize > Instructions ------------------------------------------------

void FluxPageHandler::GetInstructions(GetInstructionsCallback callback) {
  PrefService* p = profile_->GetPrefs();
  std::vector<mojom::LearnedFactPtr> learned;
  for (const base::Value& entry : p->GetList(prefs::kLearnedFacts)) {
    const base::DictValue* d = entry.GetIfDict();
    if (!d) {
      continue;
    }
    auto fact = mojom::LearnedFact::New();
    fact->id = d->FindString("id") ? *d->FindString("id") : std::string();
    fact->text = d->FindString("text") ? *d->FindString("text") : std::string();
    fact->source_run_id =
        d->FindString("run_id") ? *d->FindString("run_id") : std::string();
    fact->learned_at = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(d->FindDouble("learned_at").value_or(0)));
    learned.push_back(std::move(fact));
  }
  std::move(callback).Run(p->GetString(prefs::kInstructions),
                          std::move(learned));
}

void FluxPageHandler::SetInstructions(const std::string& text) {
  profile_->GetPrefs()->SetString(prefs::kInstructions, text);
}

void FluxPageHandler::DismissLearnedFact(const std::string& id) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kLearnedFacts);
  update->EraseIf([&id](const base::Value& entry) {
    const base::DictValue* d = entry.GetIfDict();
    const std::string* found = d ? d->FindString("id") : nullptr;
    return found && *found == id;
  });
}

// --- Customize > Skills ------------------------------------------------------

void FluxPageHandler::ListAdoptedSkills(ListAdoptedSkillsCallback callback) {
  std::vector<std::string> commands;
  for (const base::Value& entry :
       profile_->GetPrefs()->GetList(prefs::kAdoptedSkills)) {
    if (const std::string* command = entry.GetIfString()) {
      commands.push_back(*command);
    }
  }
  std::move(callback).Run(std::move(commands));
}

void FluxPageHandler::AdoptSkill(const std::string& command,
                                 const std::string& name,
                                 const std::string& description,
                                 const std::string& instructions,
                                 AdoptSkillCallback callback) {
  const std::string trimmed = std::string(
      base::TrimWhitespaceASCII(command, base::TRIM_ALL));
  if (trimmed.empty()) {
    std::move(callback).Run(false, "A skill needs a command.");
    return;
  }
  // Skills and workflows share one command namespace, so adopting over an
  // existing command would silently shadow whatever was there. Refused rather
  // than resolved: only the user knows which one they meant.
  for (const base::Value& entry :
       profile_->GetPrefs()->GetList(prefs::kAdoptedSkills)) {
    const std::string* existing = entry.GetIfString();
    if (existing && *existing == trimmed) {
      std::move(callback).Run(
          false, "/" + trimmed + " is already taken. Pick another command.");
      return;
    }
  }

  {
    ScopedDictPrefUpdate skills(profile_->GetPrefs(), prefs::kUserSkills);
    base::DictValue skill;
    skill.Set("name", name);
    skill.Set("description", description);
    skill.Set("instructions", instructions);
    skills->Set(trimmed, std::move(skill));
  }
  ScopedListPrefUpdate adopted(profile_->GetPrefs(), prefs::kAdoptedSkills);
  adopted->Append(trimmed);

  std::move(callback).Run(true, std::nullopt);
}

void FluxPageHandler::RemoveSkill(const std::string& command) {
  {
    ScopedListPrefUpdate adopted(profile_->GetPrefs(), prefs::kAdoptedSkills);
    adopted->EraseIf([&command](const base::Value& entry) {
      const std::string* existing = entry.GetIfString();
      return existing && *existing == command;
    });
  }
  ScopedDictPrefUpdate skills(profile_->GetPrefs(), prefs::kUserSkills);
  skills->Remove(command);
}

// --- Connectors -------------------------------------------------------------

mojom::ConnectorStatusPtr FluxPageHandler::ToMojom(
    const ConnectorStatus& status) const {
  auto out = mojom::ConnectorStatus::New();
  out->id = status.id;
  out->connectable = status.connectable;
  out->has_client = status.has_client;
  out->connected = status.connected;
  out->expired = status.expired;
  if (!status.detail.empty())
    out->detail = status.detail;
  return out;
}

void FluxPageHandler::ListConnectors(ListConnectorsCallback callback) {
  std::vector<mojom::ConnectorStatusPtr> out;
  if (service_ && service_->connectors()) {
    for (const ConnectorStatus& status : service_->connectors()->ListStatus())
      out.push_back(ToMojom(status));
  }
  std::move(callback).Run(std::move(out));
}

void FluxPageHandler::SetConnectorClient(const std::string& connector_id,
                                         const std::string& client_id,
                                         const std::string& client_secret,
                                         const std::string& redirect_uri,
                                         SetConnectorClientCallback callback) {
  if (!service_ || !service_->connectors()) {
    std::move(callback).Run(false, "Connector service unavailable.");
    return;
  }
  OAuthClient client;
  client.client_id = client_id;
  client.client_secret = client_secret;
  client.redirect_uri = redirect_uri;
  if (!client.valid()) {
    std::move(callback).Run(
        false, "A client id and a redirect URI are both required.");
    return;
  }
  const bool stored = service_->connectors()->SetClient(connector_id, client);
  std::move(callback).Run(
      stored, stored ? std::nullopt
                     : std::optional<std::string>(
                           "The registration could not be stored securely, so "
                           "it was discarded."));
}

void FluxPageHandler::GetConnectorClient(const std::string& connector_id,
                                         GetConnectorClientCallback callback) {
  if (!service_ || !service_->connectors()) {
    std::move(callback).Run(std::string(), std::string(), false);
    return;
  }
  const OAuthClient client = service_->connectors()->GetClient(connector_id);
  // The secret is never returned to the renderer - only whether there is one.
  // It goes in encrypted and does not come back out; a compromised console
  // cannot read a credential it is never sent.
  std::move(callback).Run(client.client_id, client.redirect_uri,
                          !client.client_secret.empty());
}

void FluxPageHandler::BeginConnect(const std::string& connector_id,
                                   BeginConnectCallback callback) {
  if (!service_ || !service_->connectors() || !web_contents_) {
    std::move(callback).Run(false, "Connector service unavailable.");
    return;
  }

  std::string error;
  const GURL url = service_->connectors()->BeginConnect(connector_id, &error);
  if (!url.is_valid()) {
    std::move(callback).Run(false, error);
    return;
  }

  content::WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (!delegate) {
    std::move(callback).Run(false, "No window to open the sign-in page in.");
    return;
  }

  // A foreground tab, and the handle to it, so the redirect can be caught and
  // the tab closed again without the user having to do either.
  content::WebContents* tab = delegate->OpenURLFromTab(
      web_contents_,
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  if (!tab) {
    std::move(callback).Run(
        false,
        "Could not open the sign-in page. Nothing was sent to the service.");
    return;
  }

  redirect_watcher_ = std::make_unique<OAuthRedirectWatcher>(
      tab, service_->connectors(),
      base::BindOnce(&FluxPageHandler::OnConnectFinished,
                     weak_factory_.GetWeakPtr(), connector_id));
  std::move(callback).Run(true, std::nullopt);
}

void FluxPageHandler::OnConnectFinished(std::string connector_id,
                                        const std::string& error) {
  redirect_watcher_.reset();
  if (!observer_ || !service_ || !service_->connectors())
    return;
  observer_->OnConnectorChanged(
      ToMojom(service_->connectors()->GetStatus(connector_id)),
      error.empty() ? std::nullopt : std::optional<std::string>(error));
}

void FluxPageHandler::SetPersonalToken(const std::string& connector_id,
                                       const std::string& token,
                                       SetPersonalTokenCallback callback) {
  if (!service_ || !service_->connectors()) {
    std::move(callback).Run(false, "Connector service unavailable.");
    return;
  }
  const bool stored =
      service_->connectors()->SetPersonalToken(connector_id, token);
  if (observer_) {
    observer_->OnConnectorChanged(
        ToMojom(service_->connectors()->GetStatus(connector_id)),
        std::nullopt);
  }
  std::move(callback).Run(
      stored, stored ? std::nullopt
                     : std::optional<std::string>(
                           "The token could not be stored securely, so it was "
                           "discarded."));
}

void FluxPageHandler::Disconnect(const std::string& connector_id) {
  if (!service_ || !service_->connectors())
    return;
  service_->connectors()->Disconnect(connector_id);
  if (observer_) {
    observer_->OnConnectorChanged(
        ToMojom(service_->connectors()->GetStatus(connector_id)),
        std::nullopt);
  }
}

void FluxPageHandler::GetSidebarCollapsed(
    GetSidebarCollapsedCallback callback) {
  std::move(callback).Run(
      profile_->GetPrefs()->GetBoolean(prefs::kSidebarCollapsed));
}

void FluxPageHandler::SetSidebarCollapsed(bool collapsed) {
  // FluxSidebarView watches this and re-lays out the window; the page only
  // restyles itself for the narrower column.
  profile_->GetPrefs()->SetBoolean(prefs::kSidebarCollapsed, collapsed);
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
