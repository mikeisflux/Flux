// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/webui/flux_page_handler.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/flux/connectors/connector_service.h"
#include "chrome/browser/flux/scheduler/workflow_scheduler.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace flux {

namespace {

// A workflow's command is how it is invoked from the palette, and it shares a
// namespace with skills. Normalised rather than rejected: a user typing
// "Weekly Report" in the dialog means /weekly-report, and making them learn
// the slug rules is not worth the round trip.
std::string NormalizeCommand(std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    if (base::IsAsciiAlphaNumeric(c)) {
      out += base::ToLowerASCII(c);
    } else if (!out.empty() && out.back() != '-') {
      out += '-';
    }
  }
  while (!out.empty() && out.back() == '-')
    out.pop_back();
  return out;
}

}  // namespace

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

void FluxPageHandler::AnswerQuestions(
    const std::string& run_id,
    std::vector<mojom::QuestionAnswerPtr> answers) {
  if (service_)
    service_->AnswerQuestions(run_id, std::move(answers));
}

void FluxPageHandler::ListPending(ListPendingCallback callback) {
  if (!service_) {
    std::move(callback).Run({}, {});
    return;
  }
  std::move(callback).Run(service_->PendingApprovals(),
                          service_->PendingQuestions());
}

void FluxPageHandler::GetAskThread(GetAskThreadCallback callback) {
  if (!service_) {
    std::move(callback).Run({}, false, nullptr);
    return;
  }
  AskSession* ask = service_->ask();
  std::move(callback).Run(ask->Thread(), ask->busy(), ask->PendingQuestion());
}

void FluxPageHandler::SendAsk(const std::string& message,
                              const std::string& model,
                              std::vector<mojom::AskAttachmentPtr> attachments) {
  if (service_)
    service_->ask()->Send(message, model, std::move(attachments));
}

void FluxPageHandler::NewAskThread() {
  if (service_)
    service_->ask()->Reset();
}

void FluxPageHandler::SetAskPanelOpen(bool open) {
  profile_->GetPrefs()->SetBoolean(prefs::kAskPanelOpen, open);
}

void FluxPageHandler::ListUserTemplates(
    ListUserTemplatesCallback callback) {
  std::vector<mojom::UserTemplatePtr> out;
  for (const auto [id, value] :
       profile_->GetPrefs()->GetDict(prefs::kUserTemplates)) {
    const base::DictValue* dict = value.GetIfDict();
    if (!dict)
      continue;
    auto item = mojom::UserTemplate::New();
    item->id = id;
    if (const std::string* v = dict->FindString("title"))
      item->title = *v;
    if (const std::string* v = dict->FindString("outcome"))
      item->outcome = *v;
    if (const std::string* v = dict->FindString("category"))
      item->category = *v;
    if (const std::string* v = dict->FindString("prompt"))
      item->prompt = *v;
    item->write_scope = static_cast<mojom::WriteScope>(
        dict->FindInt("write_scope")
            .value_or(static_cast<int>(mojom::WriteScope::kReadOnly)));
    out.push_back(std::move(item));
  }
  std::move(callback).Run(std::move(out));
}

void FluxPageHandler::SaveUserTemplate(mojom::UserTemplatePtr item,
                                       SaveUserTemplateCallback callback) {
  if (!item || item->title.empty() || item->prompt.empty()) {
    std::move(callback).Run(std::nullopt,
                            "A template needs a title and a prompt.");
    return;
  }
  // The id is derived from the title when the caller does not supply one, so
  // saving the same template twice edits it rather than making a duplicate.
  std::string id = NormalizeCommand(item->id.empty() ? item->title : item->id);
  if (id.empty()) {
    std::move(callback).Run(std::nullopt,
                            "That title has no characters an id can use.");
    return;
  }

  base::DictValue entry;
  entry.Set("title", item->title);
  entry.Set("outcome", item->outcome);
  entry.Set("category", item->category.empty() ? "Ops" : item->category);
  entry.Set("prompt", item->prompt);
  entry.Set("write_scope", static_cast<int>(item->write_scope));

  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kUserTemplates);
  update->Set(id, std::move(entry));
  std::move(callback).Run(id, std::nullopt);
}

void FluxPageHandler::DeleteUserTemplate(const std::string& id) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kUserTemplates);
  update->Remove(id);
}

void FluxPageHandler::AnswerAsk(
    std::vector<mojom::QuestionAnswerPtr> answers) {
  if (service_)
    service_->ask()->Answer(std::move(answers));
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
    if (auto it = last_errors_.find(ProviderId(provider));
        it != last_errors_.end() && !it->second.empty()) {
      status->last_error = it->second;
    }
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
                   self->last_errors_[ProviderId(provider)] = error;
                   std::move(cb).Run(false, error);
                   return;
                 }
                 SetApiKey(self->profile_, ProviderId(provider), key);
                 self->validated_.insert(ProviderId(provider));
                 self->last_errors_.erase(ProviderId(provider));
                 std::move(cb).Run(true, std::nullopt);
               },
               weak_factory_.GetWeakPtr(), provider, key, std::move(callback)));
}

void FluxPageHandler::ClearProviderKey(mojom::Provider provider) {
  SetApiKey(profile_, ProviderId(provider), std::string());
  validated_.erase(ProviderId(provider));
  // Otherwise the row for a provider with no key at all still explains why the
  // key that used to be there stopped working.
  last_errors_.erase(ProviderId(provider));
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
                   if (ok) {
                     self->validated_.insert(ProviderId(provider));
                     self->last_errors_.erase(ProviderId(provider));
                   } else {
                     self->validated_.erase(ProviderId(provider));
                     self->last_errors_[ProviderId(provider)] = error;
                   }
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
  switch (status.auth) {
    case AuthType::kOAuth2:
      out->auth = mojom::ConnectorAuth::kOAuth2;
      break;
    case AuthType::kApiKey:
      out->auth = mojom::ConnectorAuth::kApiKey;
      break;
    case AuthType::kLocal:
    case AuthType::kMcp:
    case AuthType::kUnsupported:
      out->auth = mojom::ConnectorAuth::kNone;
      break;
  }
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

namespace {

mojom::WorkflowSummaryPtr ToSummary(const Workflow& workflow) {
  auto summary = mojom::WorkflowSummary::New();
  summary->id = workflow.id;
  summary->command = workflow.command;
  summary->name = workflow.name;
  summary->description = workflow.description;
  summary->cron = workflow.cron;
  summary->schedule_display = workflow.schedule_display;
  // Null rather than the epoch, so the row can say "Never run" instead of
  // rendering 1601.
  if (!workflow.last_run.is_null())
    summary->last_run = workflow.last_run;
  if (!workflow.next_run.is_null())
    summary->next_run = workflow.next_run;
  summary->enabled = workflow.enabled;
  summary->last_fire_missed = workflow.last_fire_missed;
  summary->write_scope = workflow.spec ? workflow.spec->write_scope
                                       : mojom::WriteScope::kReadOnly;
  return summary;
}

}  // namespace

void FluxPageHandler::GetRun(const std::string& run_id,
                             GetRunCallback callback) {
  if (!service_) {
    std::move(callback).Run(nullptr, {}, std::nullopt, {});
    return;
  }
  mojom::RunProgressPtr progress = service_->GetProgress(run_id);
  std::vector<mojom::ActionRecordPtr> actions = service_->GetActions(run_id);
  const std::string summary = service_->GetSummary(run_id);
  std::move(callback).Run(
      std::move(progress), std::move(actions),
      summary.empty() ? std::optional<std::string>() : summary,
      service_->GetArtifacts(run_id));
}

void FluxPageHandler::SendFollowUp(const std::string& run_id,
                                   const std::string& text,
                                   SendFollowUpCallback callback) {
  if (!service_) {
    std::move(callback).Run(false, "Agent service unavailable.");
    return;
  }
  std::string error;
  const bool accepted = service_->SendFollowUp(run_id, text, &error);
  std::move(callback).Run(
      accepted, accepted ? std::optional<std::string>() : error);
}

void FluxPageHandler::ListWorkflows(ListWorkflowsCallback callback) {
  std::vector<mojom::WorkflowSummaryPtr> out;
  if (service_ && service_->scheduler()) {
    for (const Workflow* workflow : service_->scheduler()->List())
      out.push_back(ToSummary(*workflow));
  }
  std::move(callback).Run(std::move(out));
}

void FluxPageHandler::SaveWorkflow(mojom::WorkflowDraftPtr draft,
                                   SaveWorkflowCallback callback) {
  if (!service_ || !service_->scheduler() || !draft || !draft->spec) {
    std::move(callback).Run(std::nullopt, "Agent service unavailable.");
    return;
  }

  const std::string command = NormalizeCommand(draft->command);
  if (command.empty()) {
    std::move(callback).Run(std::nullopt,
                            "A workflow needs a command to run it by.");
    return;
  }
  if (draft->spec->prompt.empty()) {
    std::move(callback).Run(
        std::nullopt, "A workflow needs instructions - there is nothing to run.");
    return;
  }

  // Commands are one namespace shared with skills, so a collision would
  // shadow whichever the palette resolved second. Checked against other
  // workflows here; the skill registry owns its own half.
  for (const Workflow* existing : service_->scheduler()->List()) {
    if (existing->command == command && existing->id != draft->id) {
      std::move(callback).Run(
          std::nullopt,
          base::StrCat({"/", command, " is already taken by another workflow."}));
      return;
    }
  }

  // An unparseable cron is rejected rather than saved as never-firing. A
  // workflow that silently never runs is the worst outcome here: it looks
  // saved, and the first sign of trouble is the report that never arrives.
  if (!draft->cron.empty() &&
      !WorkflowScheduler::NextFireTime(draft->cron, base::Time::Now())) {
    std::move(callback).Run(
        std::nullopt,
        base::StrCat({"\"", draft->cron,
                      "\" is not a schedule this can read. Five fields, "
                      "minute first."}));
    return;
  }

  Workflow workflow;
  workflow.id = draft->id;
  workflow.command = command;
  workflow.name = draft->name;
  workflow.description = draft->description;
  workflow.cron = draft->cron;
  workflow.schedule_display = draft->schedule_display;
  workflow.enabled = draft->enabled;
  workflow.spec = std::move(draft->spec);
  if (const Workflow* previous = service_->scheduler()->Get(workflow.id)) {
    // Editing keeps the history. Losing "last run" because a description was
    // corrected would be its own small betrayal.
    workflow.last_run = previous->last_run;
    workflow.last_fire_missed = previous->last_fire_missed;
  }

  const std::string id = service_->scheduler()->Add(std::move(workflow));
  std::move(callback).Run(id, std::nullopt);
}

void FluxPageHandler::DeleteWorkflow(const std::string& workflow_id) {
  if (service_ && service_->scheduler())
    service_->scheduler()->Remove(workflow_id);
}

void FluxPageHandler::SetWorkflowEnabled(const std::string& workflow_id,
                                         bool enabled) {
  if (service_ && service_->scheduler())
    service_->scheduler()->SetEnabled(workflow_id, enabled);
}

void FluxPageHandler::RunWorkflowNow(const std::string& workflow_id,
                                     RunWorkflowNowCallback callback) {
  if (!service_ || !service_->scheduler()) {
    std::move(callback).Run(std::nullopt, "Agent service unavailable.");
    return;
  }
  const Workflow* workflow = service_->scheduler()->Get(workflow_id);
  if (!workflow || !workflow->spec) {
    std::move(callback).Run(std::nullopt, "That workflow no longer exists.");
    return;
  }

  // Runs the same spec the schedule would, and deliberately does NOT touch
  // last_run or next_run: this is a manual run, and folding it into the
  // schedule's history would make a paused workflow look like it fired.
  std::string error;
  std::optional<std::string> run_id =
      service_->StartRun(workflow->spec->Clone(), &error);
  if (!run_id) {
    std::move(callback).Run(std::nullopt, error);
    return;
  }
  std::move(callback).Run(*run_id, std::nullopt);
}

void FluxPageHandler::ShowScreen(const std::string& screen) {
  if (!web_contents_) {
    return;
  }
  content::WebContentsDelegate* delegate = web_contents_->GetDelegate();
  if (!delegate) {
    return;
  }

  // The screen name is a fragment on the console's own URL, and it is checked
  // against a fixed set rather than pasted in. It arrives from a renderer, and
  // a renderer is not trusted to name a URL the browser process will then
  // navigate to - Resolve() on an unvalidated string is how a "#" turns into
  // something that is not the console at all.
  static constexpr std::string_view kScreens[] = {
      "new-task", "templates", "workflows", "connectors", "customize",
      "approvals", "settings", "agent", "search", "welcome", "run"};

  // "run/<id>" is the one screen that carries an argument. Split before
  // matching, and hold the id to the characters a run id can contain - it
  // arrives from a renderer, and everything after the slash ends up in a URL
  // the browser process then navigates to.
  std::string_view name = screen;
  std::string_view argument;
  if (const size_t slash = screen.find('/'); slash != std::string::npos) {
    name = std::string_view(screen).substr(0, slash);
    argument = std::string_view(screen).substr(slash + 1);
  }

  bool known = false;
  for (std::string_view candidate : kScreens) {
    if (candidate == name) {
      known = true;
      break;
    }
  }
  if (!known) {
    return;
  }
  for (char c : argument) {
    if (!base::IsAsciiAlphaNumeric(c) && c != '-' && c != '_') {
      return;
    }
  }

  const GURL url =
      GURL(chrome::kChromeUIFluxURL).Resolve(base::StrCat({"#", screen}));
  if (!url.is_valid()) {
    return;
  }

  // CURRENT_TAB: the console's nav is the window's nav. The sidebar is chrome
  // that stays put and the tab beside it is the content area, so clicking
  // Templates should show Templates there rather than opening a second console
  // tab beside the first.
  delegate->OpenURLFromTab(
      web_contents_,
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

void FluxPageHandler::SetSidebarCollapsed(bool collapsed) {
  // FluxSidebarView watches this and re-lays out the window; the page only
  // restyles itself for the narrower column.
  profile_->GetPrefs()->SetBoolean(prefs::kSidebarCollapsed, collapsed);
}

void FluxPageHandler::OnAskTurn(const mojom::AskTurn& turn, bool busy) {
  observer_->OnAskTurn(turn.Clone(), busy);
}

void FluxPageHandler::OnAskQuestions(const mojom::QuestionRequest& request) {
  observer_->OnAskQuestions(request.Clone());
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

void FluxPageHandler::OnQuestionsAsked(
    const mojom::QuestionRequest& request) {
  if (observer_)
    observer_->OnQuestionsAsked(request.Clone());
}

void FluxPageHandler::OnLearnedFact(const std::string& fact,
                                    const std::string& source_run_id) {
  if (observer_)
    observer_->OnLearnedFact(fact, source_run_id);
}

void FluxPageHandler::OnRunArtifact(const std::string& run_id,
                                    const mojom::RunArtifact& artifact) {
  observer_->OnArtifact(run_id, artifact.Clone());
}

void FluxPageHandler::OnRunFinished(const std::string& run_id,
                                    mojom::RunState state,
                                    const std::string& summary) {
  observer_->OnRunFinished(run_id, state, summary);
}

}  // namespace flux
