// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_
#define CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"

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
    virtual void OnRunArtifact(const std::string& run_id,
                               const mojom::RunArtifact& artifact) {}
    virtual void OnRunFinished(const std::string& run_id,
                               mojom::RunState state,
                               const std::string& summary) {}
  };

  explicit FluxAgentService(Profile* profile);

  // The scheduler needs prefs to persist saved workflows across restarts.
  Profile* profile() { return profile_; }

  // Saved workflows. The console reaches these through FluxPageHandler.
  WorkflowScheduler* scheduler() { return scheduler_.get(); }
  ~FluxAgentService() override;

  FluxAgentService(const FluxAgentService&) = delete;
  FluxAgentService& operator=(const FluxAgentService&) = delete;

  // Returns the new run id, or nullopt with `error` set. Runs beyond the
  // concurrency cap are queued, not rejected.
  // `parent_run_id` is set only for a subagent, and is set here rather than
  // patched onto the progress entry afterwards: StartRun pumps the queue, so
  // the run can emit its first progress before this call returns, and an
  // update that arrives without a parent puts a child row in the sidebar's
  // run list that nothing ever takes back out.
  std::optional<std::string> StartRun(mojom::TaskSpecPtr spec,
                                      std::string* error,
                                      const std::string& parent_run_id = {});

  void CancelRun(const std::string& run_id);
  void PauseRun(const std::string& run_id);
  void ResumeRun(const std::string& run_id);
  void ResolveApproval(const std::string& run_id,
                       bool approved,
                       const std::string& user_note);

  std::vector<mojom::RunProgressPtr> ListRuns() const;

  // Everything the run screen needs to draw a run it did not watch happen.
  mojom::RunProgressPtr GetProgress(const std::string& run_id) const;
  std::string GetSummary(const std::string& run_id) const;
  std::vector<mojom::RunArtifactPtr> GetArtifacts(
      const std::string& run_id) const;

  // Called by the agent's own tools. The model sets a plan before it starts
  // and records a file when it produces one; both are how the run screen says
  // what is happening rather than counting tool calls.
  void SetPlan(const std::string& run_id, std::vector<mojom::TaskStepPtr> plan);
  void AdvancePlan(const std::string& run_id,
                   uint32_t index,
                   mojom::TaskStepState state);
  void AddArtifact(const std::string& run_id, mojom::RunArtifactPtr artifact);

  // Splits work across parallel child runs.
  //
  // `callback` fires once every child has finished, with each one's closing
  // summary in the order they were requested. The parent's tool call stays
  // open until then, which is what makes this a fan-out the model can reason
  // about rather than fire-and-forget.
  // `error` non-empty means nothing was started and `summaries` is empty.
  // Kept separate from the summaries rather than smuggled in as one of them:
  // with a single child, a refusal and a result are both a one-element vector,
  // and the caller would report the refusal to the model as that child's
  // answer.
  using SubagentsCallback = base::OnceCallback<void(
      std::vector<std::string> summaries, const std::string& error)>;
  void StartSubagents(const std::string& parent_run_id,
                      std::vector<std::pair<std::string, std::string>> work,
                      SubagentsCallback callback);

  // A follow-up typed into a run's composer.
  bool SendFollowUp(const std::string& run_id,
                    const std::string& text,
                    std::string* error);
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
  void OnAction(const std::string& run_id,
                const mojom::ActionRecord& action) override;
  void OnApprovalRequired(const mojom::ApprovalRequest& request) override;
  void OnFinished(const std::string& run_id,
                  mojom::RunState state,
                  const std::string& summary) override;

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
  // The run id travels WITH its spec. It used to be recovered in PumpQueue by
  // scanning progress_ for the first kQueued entry, on the stated assumption
  // that queue order and progress insertion order agree - and progress_ is a
  // std::map keyed by UUID, so its order is lexicographic, not insertion. With
  // one queued run that is invisible; with two it pairs a spec with the wrong
  // run's id, and every action and every token lands under the wrong run.
  struct QueuedRun {
    std::string run_id;
    mojom::TaskSpecPtr spec;
  };
  base::circular_deque<QueuedRun> queue_;
  std::map<std::string, mojom::RunProgressPtr> progress_;
  std::map<std::string, std::vector<mojom::ActionRecordPtr>> actions_;
  // The closing summary, kept per run. It arrives once, on OnFinished, and
  // was previously forwarded to whoever happened to be listening and then
  // dropped - so reopening a finished run showed its steps and no answer,
  // which is the part the user actually wanted.
  std::map<std::string, std::string> summaries_;
  std::map<std::string, std::vector<mojom::RunArtifactPtr>> artifacts_;

  // Parent run -> the children it is waiting on, and what to do when they are
  // all done.
  struct SubagentBatch {
    std::vector<std::string> child_ids;
    std::vector<std::string> summaries;
    size_t remaining = 0;
    SubagentsCallback callback;
  };
  std::map<std::string, SubagentBatch> batches_;
  std::map<std::string, std::string> child_to_parent_;

  const uint32_t concurrency_limit_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<FluxAgentService> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_H_
