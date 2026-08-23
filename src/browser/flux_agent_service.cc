// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/flux_agent_service.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/byte_size.h"
#include "base/system/sys_info.h"
#include "base/uuid.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/scheduler/workflow_scheduler.h"
#include "chrome/browser/flux/skills/skill_registry.h"
#include "chrome/browser/profiles/profile.h"

namespace flux {
namespace {

// A run owns a WebContents with a live page loaded. Empirically that is the
// dominant per-run cost; the model client itself is negligible.
constexpr uint64_t kBytesPerRun = 600ull * 1024 * 1024;

// Never consume more than half of physical memory, and never run more than
// this many regardless of how much RAM is installed - beyond it the bottleneck
// is network and target-site rate limits, not local resources.
constexpr uint32_t kMaxConcurrency = 12;
constexpr uint32_t kMinConcurrency = 1;

}  // namespace

FluxAgentService::FluxAgentService(Profile* profile)
    : profile_(profile),
      skills_(std::make_unique<SkillRegistry>(profile)),
      keys_(std::make_unique<ApiKeyStore>(profile)),
      connectors_(std::make_unique<ConnectorService>(profile)),
      scheduler_(std::make_unique<WorkflowScheduler>(this)),
      concurrency_limit_(ComputeConcurrencyLimit()) {
  tools_.RegisterBuiltins();
  // After the scheduler is constructed, not inside it: LoadFromPrefs reaches
  // back through this service for the profile, and doing that from the
  // scheduler's own constructor would read a half-built object.
  scheduler_->LoadFromPrefs();
}

FluxAgentService::~FluxAgentService() = default;

// static
uint32_t FluxAgentService::ComputeConcurrencyLimit() {
  // AmountOfPhysicalMemory() was renamed and now returns base::ByteSize
  // rather than a raw int64_t.
  const uint64_t physical =
      base::SysInfo::AmountOfTotalPhysicalMemory().InBytes();
  const uint64_t budget = physical / 2;
  const uint32_t by_memory = static_cast<uint32_t>(budget / kBytesPerRun);
  return std::clamp(by_memory, kMinConcurrency, kMaxConcurrency);
}

std::optional<std::string> FluxAgentService::StartRun(mojom::TaskSpecPtr spec,
                                                      std::string* error) {
  if (spec->prompt.empty()) {
    *error = "Task prompt is empty.";
    return std::nullopt;
  }
  // Fail closed on a zero budget rather than starting a run that cannot
  // afford its first model call.
  if (spec->credit_budget == 0) {
    *error = "Task has no credit budget.";
    return std::nullopt;
  }

  const std::string run_id = base::Uuid::GenerateRandomV4().AsLowercaseString();

  auto progress = mojom::RunProgress::New();
  progress->run_id = run_id;
  progress->state = mojom::RunState::kQueued;
  progress->actions_taken = 0;
  progress_[run_id] = std::move(progress);

  queue_.push_back(std::move(spec));
  PumpQueue();
  return run_id;
}

void FluxAgentService::PumpQueue() {
  while (!queue_.empty() && runs_.size() < concurrency_limit_) {
    mojom::TaskSpecPtr spec = std::move(queue_.front());
    queue_.pop_front();

    // The run id was allocated in StartRun; find the queued entry it belongs
    // to. Queue order and progress insertion order agree.
    std::string run_id;
    for (auto& [id, p] : progress_) {
      if (p->state == mojom::RunState::kQueued) {
        run_id = id;
        break;
      }
    }
    if (run_id.empty())
      return;

    auto primary = MakeProvider(*spec->model);
    std::unique_ptr<LLMProvider> failover;
    if (spec->model->allow_failover) {
      mojom::ModelConfig alt = *spec->model;
      alt.provider = spec->model->provider == mojom::Provider::kAnthropic
                         ? mojom::Provider::kOpenAI
                         : mojom::Provider::kAnthropic;
      failover = MakeProvider(alt);
    }

    auto runner = std::make_unique<AgentRunner>(
        run_id, std::move(spec), std::move(primary), std::move(failover),
        &tools_, this);
    AgentRunner* raw = runner.get();
    runs_[run_id] = std::move(runner);
    raw->Start();
  }
}

std::unique_ptr<LLMProvider> FluxAgentService::MakeProvider(
    const mojom::ModelConfig& config) {
  switch (config.provider) {
    case mojom::Provider::kAnthropic:
      return std::make_unique<AnthropicProvider>(profile_);
    case mojom::Provider::kOpenAI:
      return std::make_unique<OpenAIProvider>(profile_);
  }
  return nullptr;
}

void FluxAgentService::CancelRun(const std::string& run_id) {
  auto it = runs_.find(run_id);
  if (it != runs_.end())
    it->second->Cancel();
}

void FluxAgentService::PauseRun(const std::string& run_id) {
  auto it = runs_.find(run_id);
  if (it != runs_.end())
    it->second->Pause();
}

void FluxAgentService::ResumeRun(const std::string& run_id) {
  auto it = runs_.find(run_id);
  if (it != runs_.end())
    it->second->Resume();
}

void FluxAgentService::ResolveApproval(const std::string& run_id,
                                       bool approved,
                                       const std::string& user_note) {
  auto it = runs_.find(run_id);
  if (it != runs_.end())
    it->second->ResolveApproval(approved, user_note);
}

std::vector<mojom::RunProgressPtr> FluxAgentService::ListRuns() const {
  std::vector<mojom::RunProgressPtr> out;
  out.reserve(progress_.size());
  for (const auto& [id, p] : progress_)
    out.push_back(p.Clone());
  return out;
}

std::vector<mojom::RunArtifactPtr> FluxAgentService::GetArtifacts(
    const std::string& run_id) const {
  std::vector<mojom::RunArtifactPtr> out;
  auto it = artifacts_.find(run_id);
  if (it == artifacts_.end())
    return out;
  out.reserve(it->second.size());
  for (const mojom::RunArtifactPtr& artifact : it->second)
    out.push_back(artifact.Clone());
  return out;
}

void FluxAgentService::SetPlan(const std::string& run_id,
                               std::vector<mojom::TaskStepPtr> plan) {
  auto it = progress_.find(run_id);
  if (it == progress_.end())
    return;
  it->second->plan = std::move(plan);
  for (Observer& o : observers_)
    o.OnRunProgress(*it->second);
}

void FluxAgentService::AdvancePlan(const std::string& run_id,
                                   uint32_t index,
                                   mojom::TaskStepState state) {
  auto it = progress_.find(run_id);
  if (it == progress_.end() || index >= it->second->plan.size())
    return;
  it->second->plan[index]->state = state;
  for (Observer& o : observers_)
    o.OnRunProgress(*it->second);
}

void FluxAgentService::AddArtifact(const std::string& run_id,
                                   mojom::RunArtifactPtr artifact) {
  if (!artifact)
    return;
  for (Observer& o : observers_)
    o.OnRunArtifact(run_id, *artifact);
  artifacts_[run_id].push_back(std::move(artifact));
}

bool FluxAgentService::SendFollowUp(const std::string& run_id,
                                    const std::string& text,
                                    std::string* error) {
  if (text.empty()) {
    *error = "Nothing to send.";
    return false;
  }
  auto it = runs_.find(run_id);
  if (it == runs_.end() || !it->second) {
    // A finished run's runner is gone. Rather than silently starting an
    // unrelated task, say so - the console offers to open a new one.
    *error = "That run has ended. Start a new task to carry on from it.";
    return false;
  }
  it->second->AddUserMessage(text);
  return true;
}

mojom::RunProgressPtr FluxAgentService::GetProgress(
    const std::string& run_id) const {
  auto it = progress_.find(run_id);
  return it == progress_.end() ? nullptr : it->second.Clone();
}

std::string FluxAgentService::GetSummary(const std::string& run_id) const {
  auto it = summaries_.find(run_id);
  return it == summaries_.end() ? std::string() : it->second;
}

std::vector<mojom::ActionRecordPtr> FluxAgentService::GetActions(
    const std::string& run_id) const {
  std::vector<mojom::ActionRecordPtr> out;
  auto it = actions_.find(run_id);
  if (it == actions_.end())
    return out;
  out.reserve(it->second.size());
  for (const auto& a : it->second)
    out.push_back(a.Clone());
  return out;
}

std::optional<std::string> FluxAgentService::CompileReplay(
    const std::string& run_id,
    std::string* error) {
  auto progress_it = progress_.find(run_id);
  if (progress_it == progress_.end()) {
    *error = "No such run.";
    return std::nullopt;
  }
  if (progress_it->second->state != mojom::RunState::kSucceeded) {
    *error = "Only a successful run can be compiled into a replay.";
    return std::nullopt;
  }
  auto actions_it = actions_.find(run_id);
  if (actions_it == actions_.end() || actions_it->second.empty()) {
    *error = "Run has no recorded actions.";
    return std::nullopt;
  }
  return scheduler_->CompileWorkflowFromActions(run_id, actions_it->second,
                                                error);
}

FluxAgentService::Concurrency FluxAgentService::GetConcurrency() const {
  return {concurrency_limit_, static_cast<uint32_t>(runs_.size()),
          static_cast<uint32_t>(queue_.size())};
}

void FluxAgentService::OnProgress(const mojom::RunProgress& progress) {
  progress_[progress.run_id] = progress.Clone();
  for (Observer& o : observers_)
    o.OnRunProgress(progress);
}

void FluxAgentService::OnAction(const mojom::ActionRecord& action) {
  // The delegate interface does not carry the run id on every callback, so the
  // runner stamps it into the record before emitting.
  for (auto& [run_id, runner] : runs_) {
    if (runner->state() == mojom::RunState::kRunning ||
        runner->state() == mojom::RunState::kAwaitingApproval) {
      actions_[run_id].push_back(action.Clone());
      for (Observer& o : observers_)
        o.OnRunAction(run_id, action);
      return;
    }
  }
}

void FluxAgentService::OnApprovalRequired(const mojom::ApprovalRequest& request) {
  for (Observer& o : observers_)
    o.OnApprovalRequested(request);
}

void FluxAgentService::OnFinished(mojom::RunState state,
                                  const std::string& summary) {
  std::string finished_id;
  for (auto& [run_id, runner] : runs_) {
    if (runner->state() == state) {
      finished_id = run_id;
      break;
    }
  }
  if (finished_id.empty())
    return;

  if (auto it = progress_.find(finished_id); it != progress_.end())
    it->second->state = state;

  // Kept, not just forwarded. The summary is the answer the user asked for,
  // and it arrives exactly once - a console opened after the run ended, or
  // reloaded, had its steps and no result.
  summaries_[finished_id] = summary;

  for (Observer& o : observers_)
    o.OnRunFinished(finished_id, state, summary);

  // Destroying the runner from inside its own callback would delete `this`
  // out from under the stack; defer.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<FluxAgentService> self, std::string id) {
            if (!self)
              return;
            self->runs_.erase(id);
            self->PumpQueue();
          },
          weak_factory_.GetWeakPtr(), finished_id));
}

void FluxAgentService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FluxAgentService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FluxAgentService::Shutdown() {
  // Cancel in-flight runs before teardown so no runner outlives the service.
  for (auto& [run_id, runner] : runs_)
    runner->Cancel();
  runs_.clear();
  queue_.clear();
  scheduler_.reset();
}

}  // namespace flux
