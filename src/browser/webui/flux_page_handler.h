// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_
#define CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
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
  void OnRunFinished(const std::string& run_id,
                     mojom::RunState state,
                     const std::string& summary) override;

  raw_ptr<Profile> profile_;
  raw_ptr<FluxAgentService> service_;
  // Providers whose stored key has been confirmed to work this session.
  std::set<std::string> validated_;
  mojo::Receiver<mojom::FluxPageHandler> receiver_;
  mojo::Remote<mojom::FluxPageHandlerObserver> observer_;
  base::ScopedObservation<FluxAgentService, FluxAgentService::Observer>
      observation_{this};
  base::WeakPtrFactory<FluxPageHandler> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_WEBUI_FLUX_PAGE_HANDLER_H_
