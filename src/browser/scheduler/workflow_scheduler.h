// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_SCHEDULER_WORKFLOW_SCHEDULER_H_
#define CHROME_BROWSER_FLUX_SCHEDULER_WORKFLOW_SCHEDULER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"

namespace flux {

class FluxAgentService;

// A saved task that runs on a schedule.
struct Workflow {
  std::string id;
  std::string command;           // reachable from the Ctrl+K palette
  std::string name;
  std::string cron;              // 5-field, local time
  mojom::TaskSpecPtr spec;
  bool enabled = true;

  base::Time last_run;
  base::Time next_run;
  // Set when a firing was missed because the machine was asleep. Surfaced in
  // run history rather than silently skipped - a workflow that never fired
  // otherwise looks identical to one that fired and found nothing.
  bool last_fire_missed = false;
};

// Fires saved workflows, and compiles finished runs into replayable ones.
class WorkflowScheduler {
 public:
  explicit WorkflowScheduler(FluxAgentService* service);
  ~WorkflowScheduler();

  WorkflowScheduler(const WorkflowScheduler&) = delete;
  WorkflowScheduler& operator=(const WorkflowScheduler&) = delete;

  std::string Add(Workflow workflow);
  void Remove(const std::string& workflow_id);
  void SetEnabled(const std::string& workflow_id, bool enabled);
  std::vector<const Workflow*> List() const;

  // Turns a successful run's action trace into a workflow that replays the
  // same steps without model inference.
  //
  // This is the main lever against per-run cost: a task that cost real money
  // to work out becomes near-free to repeat. It only applies where the trace
  // is deterministic - see CanCompile.
  std::optional<std::string> CompileWorkflowFromActions(
      const std::string& run_id,
      const std::vector<mojom::ActionRecordPtr>& actions,
      std::string* error);

  // Next fire time for `cron` after `after`, in local time. Returns nullopt
  // for an unparseable expression.
  static std::optional<base::Time> NextFireTime(const std::string& cron,
                                                base::Time after);

 private:
  // A trace is replayable only when every step is reproducible: no approval
  // gates (a human decided something), no failures (the trace includes
  // recovery that may not be needed again), and nothing that depended on
  // model judgement rather than a fixed input.
  static bool CanCompile(const std::vector<mojom::ActionRecordPtr>& actions,
                         std::string* reason);

  void ScheduleNext();
  void OnTimerFired();

  raw_ptr<FluxAgentService> service_;
  std::map<std::string, Workflow> workflows_;
  base::WallClockTimer timer_;
  base::WeakPtrFactory<WorkflowScheduler> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_SCHEDULER_WORKFLOW_SCHEDULER_H_
