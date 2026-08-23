// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_
#define CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/flux/connectors/oauth_redirect_watcher.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}

namespace flux {

// Bridges the chrome://flux renderer to FluxAgentService. Owns no state of its
// own - the service is the source of truth, so a console reload does not
// disturb a running task.
class FluxPageHandler : public mojom::FluxPageHandler,
                        public FluxAgentService::Observer {
 public:
  FluxPageHandler(mojo::PendingReceiver<mojom::FluxPageHandler> receiver,
                  mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
                  Profile* profile,
                  content::WebContents* web_contents);
  ~FluxPageHandler() override;

  FluxPageHandler(const FluxPageHandler&) = delete;
  FluxPageHandler& operator=(const FluxPageHandler&) = delete;

 private:
  // mojom::FluxPageHandler:
  void StartRun(mojom::TaskSpecPtr spec, StartRunCallback callback) override;
  void CancelRun(const std::string& run_id) override;
  void PauseRun(const std::string& run_id) override;
  void ResumeRun(const std::string& run_id) override;
  void ResolveApproval(const std::string& run_id,
                       bool approved,
                       const std::optional<std::string>& user_note) override;
  void ListRuns(ListRunsCallback callback) override;
  void GetActions(const std::string& run_id,
                  GetActionsCallback callback) override;
  void CompileReplay(const std::string& run_id,
                     CompileReplayCallback callback) override;
  void GetConcurrencyLimit(GetConcurrencyLimitCallback callback) override;
  void ListProviderKeys(ListProviderKeysCallback callback) override;
  void SetProviderKey(mojom::Provider provider,
                      const std::string& key,
                      SetProviderKeyCallback callback) override;
  void ClearProviderKey(mojom::Provider provider) override;
  void ValidateProviderKey(mojom::Provider provider,
                           ValidateProviderKeyCallback callback) override;
  void GetInstructions(GetInstructionsCallback callback) override;
  void SetInstructions(const std::string& text) override;
  void DismissLearnedFact(const std::string& id) override;
  void ListAdoptedSkills(ListAdoptedSkillsCallback callback) override;
  void AdoptSkill(const std::string& command,
                  const std::string& name,
                  const std::string& description,
                  const std::string& instructions,
                  AdoptSkillCallback callback) override;
  void RemoveSkill(const std::string& command) override;
  void ListConnectors(ListConnectorsCallback callback) override;
  void SetConnectorClient(const std::string& connector_id,
                          const std::string& client_id,
                          const std::string& client_secret,
                          const std::string& redirect_uri,
                          SetConnectorClientCallback callback) override;
  void GetConnectorClient(const std::string& connector_id,
                          GetConnectorClientCallback callback) override;
  void BeginConnect(const std::string& connector_id,
                    BeginConnectCallback callback) override;
  void SetPersonalToken(const std::string& connector_id,
                        const std::string& token,
                        SetPersonalTokenCallback callback) override;
  void Disconnect(const std::string& connector_id) override;
  void GetSidebarCollapsed(GetSidebarCollapsedCallback callback) override;
  void SetSidebarCollapsed(bool collapsed) override;
  void ShowScreen(const std::string& screen) override;
  void GetRun(const std::string& run_id, GetRunCallback callback) override;
  void SendFollowUp(const std::string& run_id,
                    const std::string& text,
                    SendFollowUpCallback callback) override;
  void ListWorkflows(ListWorkflowsCallback callback) override;
  void SaveWorkflow(mojom::WorkflowDraftPtr draft,
                    SaveWorkflowCallback callback) override;
  void DeleteWorkflow(const std::string& workflow_id) override;
  void SetWorkflowEnabled(const std::string& workflow_id,
                          bool enabled) override;
  void RunWorkflowNow(const std::string& workflow_id,
                      RunWorkflowNowCallback callback) override;

  // Turns a ConnectorStatus into the mojom struct the console renders.
  mojom::ConnectorStatusPtr ToMojom(const ConnectorStatus& status) const;

  // Called by the redirect watcher when an authorization finishes, one way or
  // the other.
  void OnConnectFinished(std::string connector_id, const std::string& error);

  // Issues a minimal completion to confirm the key is accepted. Shared by
  // SetProviderKey and ValidateProviderKey.
  void ProbeKey(mojom::Provider provider,
                const std::string& key,
                base::OnceCallback<void(bool, std::string)> done);

  // FluxAgentService::Observer:
  void OnRunProgress(const mojom::RunProgress& progress) override;
  void OnRunAction(const std::string& run_id,
                   const mojom::ActionRecord& action) override;
  void OnApprovalRequested(const mojom::ApprovalRequest& request) override;
  void OnLearnedFact(const std::string& fact,
                     const std::string& source_run_id) override;
  void OnRunArtifact(const std::string& run_id,
                     const mojom::RunArtifact& artifact) override;
  void OnRunFinished(const std::string& run_id,
                     mojom::RunState state,
                     const std::string& summary) override;

  raw_ptr<Profile> profile_;
  raw_ptr<FluxAgentService> service_;
  raw_ptr<content::WebContents> web_contents_;
  // Alive only while an authorization tab is open. Destroying it stops the
  // watch, which is what closing the console mid-flow should do.
  std::unique_ptr<OAuthRedirectWatcher> redirect_watcher_;
  // Providers whose stored key has been confirmed to work this session.
  std::set<std::string> validated_;
  // Why a key last failed, per provider. ProviderKeyStatus has always carried
  // the field and nothing ever wrote it, so a key that stopped working showed
  // "not verified" with no reason once the user navigated away from the
  // message the failing call produced.
  std::map<std::string, std::string> last_errors_;
  mojo::Receiver<mojom::FluxPageHandler> receiver_;
  mojo::Remote<mojom::FluxPageHandlerObserver> observer_;
  base::ScopedObservation<FluxAgentService, FluxAgentService::Observer>
      observation_{this};
  base::WeakPtrFactory<FluxPageHandler> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_
