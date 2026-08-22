// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_
#define CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/agent/agent_runner.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/connectors/connector_service.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace flux {

class SkillRegistry;
class WorkflowScheduler;

// Owns every agent run for a profile. One instance per Profile, created by
// FluxAgentServiceFactory.
//
// Concurrency is bounded here rather than in the UI. The reference product
// sizes its pool to available memory ("Parallel tasks: sized to this PC's
// memory") and so does this, but it also exposes the resolved number and the
// queue depth, which the reference does not — a user whose task is sitting in
// a queue has no way to tell that apart from a task that is running slowly.
class FluxAgentService : public KeyedService, public AgentRunner::Delegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRunProgress(const mojom::RunProgress& progress) {}
    virtual void OnRunAction(const std::string& run_id,
                             const mojom::ActionRecord& action) {}
    virtual void OnApprovalRequested(const mojom::ApprovalRequest& request) {}
    virtual void OnRunFinished(const std::string& run_id,
                               mojom::RunState state,
                               const std::string& summary) {}
  };

  explicit FluxAgentService(Profile* profile);
  ~FluxAgentService() override;

  FluxAgentService(const FluxAgentService&) = delete;
  FluxAgentService& operator=(const FluxAgentService&) = delete;

  // Returns the new run id, or nullopt with `error` set. Runs beyond the
  // concurrency cap are queued, not rejected.
  std::optional<std::string> StartRun(mojom::TaskSpecPtr spec,
                                      std::string* error);

  void CancelRun(const std::string& run_id);
  void PauseRun(const std::string& run_id);
  void ResumeRun(const std::string& run_id);
  void ResolveApproval(const std::string& run_id,
                       bool approved,
                       const std::string& user_note);

  std::vector<mojom::RunProgressPtr> ListRuns() const;
  std::vector<mojom::ActionRecordPtr> GetActions(const std::string& run_id) const;

  // Compiles a finished run's action trace into a replayable workflow.
  // Returns the workflow id, or nullopt if the run did not succeed.
  std::optional<std::string> CompileReplay(const std::string& run_id,
                                           std::string* error);

  struct Concurrency {
    uint32_t limit = 0;
    uint32_t active = 0;
    uint32_t queued = 0;
  };
  Concurrency GetConcurrency() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  SkillRegistry* skills() { return skills_.get(); }
  ApiKeyStore* keys() { return keys_.get(); }
  ConnectorService* connectors() { return connectors_.get(); }
  ToolRegistry* tools() { return &tools_; }

  // KeyedService:
  void Shutdown() override;

 private:
  // AgentRunner::Delegate:
  void OnProgress(const mojom::RunProgress& progress) override;
  void OnAction(const mojom::ActionRecord& action) override;
  void OnApprovalRequired(const mojom::ApprovalRequest& request) override;
  void OnFinished(mojom::RunState state, const std::string& summary) override;

  // Starts queued runs until the cap is reached. Called on every completion.
  void PumpQueue();

  // Derives the concurrency cap from installed RAM. Each run owns a WebContents
  // with a live page, which is the dominant cost — budget ~600MB per run and
  // never take more than half of physical memory.
  static uint32_t ComputeConcurrencyLimit();

  std::unique_ptr<LLMProvider> MakeProvider(const mojom::ModelConfig& config);

  raw_ptr<Profile> profile_;
  ToolRegistry tools_;
  std::unique_ptr<SkillRegistry> skills_;
  std::unique_ptr<ApiKeyStore> keys_;
  std::unique_ptr<ConnectorService> connectors_;
  std::unique_ptr<WorkflowScheduler> scheduler_;

  std::map<std::string, std::unique_ptr<AgentRunner>> runs_;
  base::circular_deque<mojom::TaskSpecPtr> queue_;
  std::map<std::string, mojom::RunProgressPtr> progress_;
  std::map<std::string, std::vector<mojom::ActionRecordPtr>> actions_;

  const uint32_t concurrency_limit_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<FluxAgentService> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_
