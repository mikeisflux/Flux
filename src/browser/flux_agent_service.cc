// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/flux_agent_service.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/providers/provider_keys.h"
#include "chrome/browser/flux/scheduler/workflow_scheduler.h"
#include "chrome/browser/flux/skills/skill_registry.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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

// A short, stable name for a run, from the first thing the prompt asks for.
//
// The prompt is the only thing available at StartRun - the model has not run
// yet - and its first line is almost always the instruction. Templates begin
// with an imperative ("Go to my open LinkedIn search results tab."), which
// makes a serviceable title once it is cut to length on a word boundary.
std::string TitleFor(const std::string& prompt) {
  std::string line = prompt.substr(0, prompt.find('\n'));
  base::TrimWhitespaceASCII(line, base::TRIM_ALL, &line);
  if (line.empty())
    return "Untitled task";

  constexpr size_t kMax = 48;
  if (line.size() <= kMax)
    return line;
  // Cut on a space so the label does not end mid-word; fall back to a hard cut
  // for a 48-character run with no spaces in it.
  size_t cut = line.rfind(' ', kMax);
  if (cut == std::string::npos || cut < kMax / 2)
    cut = kMax;
  return base::StrCat({line.substr(0, cut), "\u2026"});
}

}  // namespace

FluxAgentService::FluxAgentService(Profile* profile)
    : profile_(profile),
      skills_(std::make_unique<SkillRegistry>(profile)),
      keys_(std::make_unique<ApiKeyStore>(profile)),
      connectors_(std::make_unique<ConnectorService>(profile)),
      scheduler_(std::make_unique<WorkflowScheduler>(this)),
      concurrency_limit_(ComputeConcurrencyLimit()) {
  tools_.RegisterBuiltins(this);
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

std::optional<std::string> FluxAgentService::StartRun(
    mojom::TaskSpecPtr spec,
    std::string* error,
    const std::string& parent_run_id) {
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
  progress->title = TitleFor(spec->prompt);
  progress->started_at = base::Time::Now();
  progress->state = mojom::RunState::kQueued;
  progress->actions_taken = 0;
  if (!parent_run_id.empty())
    progress->parent_run_id = parent_run_id;
  progress_[run_id] = std::move(progress);

  queue_.push_back(QueuedRun{run_id, std::move(spec)});
  PumpQueue();
  return run_id;
}

void FluxAgentService::PumpQueue() {
  while (!queue_.empty() && runs_.size() < concurrency_limit_) {
    const std::string run_id = std::move(queue_.front().run_id);
    mojom::TaskSpecPtr spec = std::move(queue_.front().spec);
    queue_.pop_front();

    auto primary = MakeProvider(*spec->model);
    std::unique_ptr<LLMProvider> failover;
    if (spec->model->allow_failover) {
      mojom::ModelConfig alt = *spec->model;
      alt.provider = spec->model->provider == mojom::Provider::kAnthropic
                         ? mojom::Provider::kOpenAI
                         : mojom::Provider::kAnthropic;
      // Only if the other provider actually has a key. Building one without
      // makes failover worse than none: a rate limit, which is temporary and
      // says so, gets replaced by "No OpenAI API key configured", which is
      // both wrong about the cause and not retryable.
      const std::string other =
          GetApiKey(profile_, alt.provider == mojom::Provider::kAnthropic
                                  ? "anthropic"
                                  : "openai");
      if (!other.empty())
        failover = MakeProvider(alt);
    }

    auto runner = std::make_unique<AgentRunner>(
        run_id, profile_, std::move(spec), std::move(primary),
        std::move(failover), &tools_, this);
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
  // Top-level runs only. Four children of one task are one thing happening,
  // not five, and the console draws them inside their parent's run.
  std::vector<mojom::RunProgressPtr> out;
  out.reserve(progress_.size());
  for (const auto& [id, p] : progress_) {
    if (p->parent_run_id && !p->parent_run_id->empty())
      continue;
    out.push_back(p.Clone());
  }
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

void FluxAgentService::StartSubagents(
    const std::string& parent_run_id,
    std::vector<std::pair<std::string, std::string>> work,
    SubagentsCallback callback) {
  auto parent = progress_.find(parent_run_id);
  if (work.empty() || parent == progress_.end()) {
    std::move(callback).Run({}, "No work was given, or the run has ended.");
    return;
  }

  // One level only. A child inherits the parent's tool set, so without this a
  // subagent can spawn subagents that spawn subagents - each split of the
  // budget still rounds up to at least one credit, so the recursion is not
  // even bounded by cost.
  if (parent->second->parent_run_id &&
      !parent->second->parent_run_id->empty()) {
    std::move(callback).Run({}, "A subagent cannot spawn subagents of its own.");
    return;
  }

  // One batch at a time. Overwriting batches_[parent] would drop the first
  // batch's callback on the floor, and the tool call still holding it would
  // never return - the parent run would sit at "thinking" until its budget
  // ran out.
  if (batches_.count(parent_run_id)) {
    std::move(callback).Run(
        {}, "Subagents from a previous call are still running.");
    return;
  }

  // The console draws a fixed set of rows and a parent farming out fifty
  // children is a runaway, not a plan. Refused rather than truncated: silently
  // dropping the tail returns nine labels' worth of intent and eight answers,
  // and the model has no way to tell which one it never got.
  constexpr size_t kMaxSubagents = 8;
  if (work.size() > kMaxSubagents) {
    std::move(callback).Run(
        {}, base::StrCat({"At most ", base::NumberToString(kMaxSubagents),
                          " subagents at a time. Split the work into fewer "
                          "pieces, or do some of it yourself."}));
    return;
  }

  // Children inherit the parent's model and scope. A child that could reach
  // further than the task it was spawned from would be a hole straight
  // through WriteScope - the approval the user gave was for this task, not
  // for whatever it decides to delegate.
  const AgentRunner* parent_runner = nullptr;
  if (auto it = runs_.find(parent_run_id); it != runs_.end())
    parent_runner = it->second.get();

  // child_ids is parallel to `work`, one slot per requested child, empty for
  // any that could not start. Pushing only the successes would slide the
  // indices and file a child's result under a sibling's heading.
  SubagentBatch batch;
  batch.child_ids.assign(work.size(), std::string());
  batch.summaries.resize(work.size());
  batch.remaining = work.size();
  batch.callback = std::move(callback);

  for (size_t i = 0; i < work.size(); ++i) {
    auto spec = mojom::TaskSpec::New();
    spec->prompt = work[i].second;
    spec->write_scope = mojom::WriteScope::kReadOnly;
    if (parent_runner && parent_runner->spec()) {
      spec->write_scope = parent_runner->spec()->write_scope;
      spec->model = parent_runner->spec()->model->Clone();
      // The budget is split, not copied. Four children each inheriting the
      // parent's ceiling is a task that can cost five times what the user
      // agreed to. Never down to zero, though: a child
      // given nothing fails on its first turn and reports a budget error
      // rather than the work it was asked to do.
      spec->credit_budget = std::max<uint64_t>(
          1u, parent_runner->spec()->credit_budget / (work.size() + 1));
      spec->profile_id = parent_runner->spec()->profile_id;
    } else {
      spec->model = mojom::ModelConfig::New();
      spec->model->provider = mojom::Provider::kAnthropic;
      spec->model->model = "claude-sonnet-5";
      spec->model->max_output_tokens = 8192;
      spec->credit_budget = 1;
    }

    std::string error;
    std::optional<std::string> child =
        StartRun(std::move(spec), &error, parent_run_id);
    if (!child) {
      batch.summaries[i] = base::StrCat({"Could not start: ", error});
      batch.remaining--;
      continue;
    }

    batch.child_ids[i] = *child;
    child_to_parent_[*child] = parent_run_id;

    auto summary = mojom::SubagentSummary::New();
    summary->run_id = *child;
    summary->label = work[i].first;
    summary->state = mojom::RunState::kQueued;
    parent->second->subagents.push_back(std::move(summary));
  }

  batches_[parent_run_id] = std::move(batch);
  for (Observer& o : observers_)
    o.OnRunProgress(*parent->second);

  // Every child failed to start. Nothing will call back, so do it here.
  if (batches_[parent_run_id].remaining == 0) {
    SubagentBatch done = std::move(batches_[parent_run_id]);
    batches_.erase(parent_run_id);
    std::move(done.callback).Run(std::move(done.summaries), std::string());
  }
}

void FluxAgentService::RememberFact(const std::string& run_id,
                                    const std::string& text) {
  if (text.empty() || !profile_)
    return;

  // Same shape the console reads back in GetInstructions: {id, text, run_id,
  // learned_at}. Written here rather than in the page handler because a run
  // can outlive the console tab that started it, and a fact learned after the
  // user closed the console must still be kept.
  base::DictValue fact;
  fact.Set("id", base::Uuid::GenerateRandomV4().AsLowercaseString());
  fact.Set("text", text);
  fact.Set("run_id", run_id);
  fact.Set("learned_at", static_cast<double>(
                             base::Time::Now()
                                 .ToDeltaSinceWindowsEpoch()
                                 .InMicroseconds()));

  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kLearnedFacts);
  // Deduplicated on the text: an agent that relearns the same thing on every
  // run would otherwise fill the list with one fact repeated fifty times.
  for (const base::Value& entry : *update) {
    if (entry.is_dict()) {
      const std::string* existing = entry.GetDict().FindString("text");
      if (existing && *existing == text)
        return;
    }
  }
  update->Append(std::move(fact));

  for (Observer& o : observers_)
    o.OnLearnedFact(text, run_id);
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
  // The runner builds its progress from scratch every turn and knows nothing
  // about the fields the service owns, so they are carried over rather than
  // reset to empty on every update.
  //
  // The plan is one of them, and it mattered most: set_plan wrote it here, the
  // very next model turn overwrote it with an empty one, and the panel the
  // whole feature exists for appeared for a fraction of a second and vanished.
  auto existing = progress_.find(progress.run_id);
  mojom::RunProgressPtr next = progress.Clone();
  if (existing != progress_.end()) {
    next->title = existing->second->title;
    next->started_at = existing->second->started_at;
    next->plan = std::move(existing->second->plan);
    next->subagents = std::move(existing->second->subagents);
    next->parent_run_id = existing->second->parent_run_id;
  }
  const std::optional<std::string> parent_id = next->parent_run_id;
  const std::string run_id = next->run_id;
  const std::optional<std::string> current_step = next->current_step;
  const mojom::RunState state = next->state;
  progress_[run_id] = std::move(next);

  for (Observer& o : observers_)
    o.OnRunProgress(*progress_[run_id]);

  // A child's row in the parent shows what the child is doing. Without this
  // the parent's panel says "4 running" and nothing else for the whole batch.
  if (!parent_id || parent_id->empty())
    return;
  auto parent = progress_.find(*parent_id);
  if (parent == progress_.end())
    return;
  bool changed = false;
  for (mojom::SubagentSummaryPtr& child : parent->second->subagents) {
    if (child->run_id != run_id)
      continue;
    child->state = state;
    child->current_step = current_step;
    changed = true;
  }
  if (!changed)
    return;
  for (Observer& o : observers_)
    o.OnRunProgress(*parent->second);
}

void FluxAgentService::OnAction(const std::string& run_id,
                                const mojom::ActionRecord& action) {
  actions_[run_id].push_back(action.Clone());
  for (Observer& o : observers_)
    o.OnRunAction(run_id, action);
}

void FluxAgentService::OnApprovalRequired(const mojom::ApprovalRequest& request) {
  for (Observer& o : observers_)
    o.OnApprovalRequested(request);
}

void FluxAgentService::OnQuestionsAsked(
    const mojom::QuestionRequest& request) {
  // The run's own state changed to kAwaitingInput before this fired, so the
  // console's run list shows it as waiting on the user rather than as running
  // with nothing happening.
  if (auto it = progress_.find(request.run_id); it != progress_.end()) {
    it->second->state = mojom::RunState::kAwaitingInput;
    for (Observer& o : observers_)
      o.OnRunProgress(*it->second);
  }
  for (Observer& o : observers_)
    o.OnQuestionsAsked(request);
}

void FluxAgentService::AskUser(const std::string& run_id,
                               std::vector<mojom::AgentQuestionPtr> questions,
                               const std::string& preamble,
                               AnswersCallback answered) {
  auto it = runs_.find(run_id);
  if (it == runs_.end() || !it->second) {
    std::move(answered).Run({});
    return;
  }
  it->second->AskUser(std::move(questions), preamble, std::move(answered));
}

void FluxAgentService::AnswerQuestions(
    const std::string& run_id,
    std::vector<mojom::QuestionAnswerPtr> answers) {
  auto it = runs_.find(run_id);
  if (it == runs_.end() || !it->second)
    return;
  it->second->ResolveQuestions(std::move(answers));
}

void FluxAgentService::OnFinished(const std::string& finished_id,
                                  mojom::RunState state,
                                  const std::string& summary) {
  if (auto it = progress_.find(finished_id); it != progress_.end())
    it->second->state = state;

  // Kept, not just forwarded. The summary is the answer the user asked for,
  // and it arrives exactly once - a console opened after the run ended, or
  // reloaded, had its steps and no result.
  summaries_[finished_id] = summary;

  // If this was a subagent, update the parent's row and, once the last child
  // is in, hand the whole batch back to the tool call that is still waiting.
  if (auto link = child_to_parent_.find(finished_id);
      link != child_to_parent_.end()) {
    const std::string parent_id = link->second;
    child_to_parent_.erase(link);

    if (auto parent = progress_.find(parent_id); parent != progress_.end()) {
      for (mojom::SubagentSummaryPtr& child : parent->second->subagents) {
        if (child->run_id != finished_id)
          continue;
        child->state = state;
        child->current_step = std::nullopt;
      }
      for (Observer& o : observers_)
        o.OnRunProgress(*parent->second);
    }

    if (auto batch = batches_.find(parent_id); batch != batches_.end()) {
      for (size_t i = 0; i < batch->second.child_ids.size(); ++i) {
        if (batch->second.child_ids[i] == finished_id)
          batch->second.summaries[i] = summary;
      }
      if (--batch->second.remaining == 0) {
        SubagentBatch done = std::move(batch->second);
        batches_.erase(batch);
        std::move(done.callback).Run(std::move(done.summaries), std::string());
      }
    }
  }

  // The parent ended with children still out - cancelled, out of budget, or
  // failed. Nothing will ever read their results, so stop them rather than
  // leaving runs going that no screen shows and no callback is waiting on.
  if (auto batch = batches_.find(finished_id); batch != batches_.end()) {
    SubagentBatch orphaned = std::move(batch->second);
    batches_.erase(batch);
    for (const std::string& child_id : orphaned.child_ids) {
      if (child_id.empty())
        continue;
      child_to_parent_.erase(child_id);
      if (auto it = runs_.find(child_id); it != runs_.end())
        it->second->Cancel();
    }
    std::move(orphaned.callback).Run(std::move(orphaned.summaries),
                                     std::string());
  }

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
