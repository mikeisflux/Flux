// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/agent_runner.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"

namespace flux {
namespace {

// Stop conditions. Without them an agent loops until the budget is gone, which
// is both expensive and the most common way these fail in practice.
constexpr uint32_t kMaxConsecutiveFailures = 3;
constexpr uint32_t kRepeatWindow = 6;
constexpr uint32_t kMaxRepeatsInWindow = 3;
constexpr size_t kMaxActions = 200;

int ScopeRank(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return 0;
    case mojom::WriteScope::kDraft:    return 1;
    case mojom::WriteScope::kSend:     return 2;
    case mojom::WriteScope::kPurchase: return 3;
  }
  return 3;
}

// A digest of tool + inputs, used to notice the agent repeating itself.
std::string DigestOf(const ToolCall& call) {
  std::string json;
  base::JSONWriter::Write(call.input, &json);
  return base::SHA1HashString(base::StrCat({call.name, "|", json}));
}

}  // namespace

AgentRunner::AgentRunner(std::string run_id,
                         mojom::TaskSpecPtr spec,
                         std::unique_ptr<LLMProvider> provider,
                         std::unique_ptr<LLMProvider> failover,
                         ToolRegistry* tools,
                         Delegate* delegate)
    : run_id_(std::move(run_id)),
      spec_(std::move(spec)),
      provider_(std::move(provider)),
      failover_(std::move(failover)),
      tools_(tools),
      delegate_(delegate) {}

AgentRunner::~AgentRunner() = default;

void AgentRunner::Start() {
  state_ = mojom::RunState::kRunning;

  Message task;
  task.role = Message::Role::kUser;
  task.text = spec_->prompt;
  history_.push_back(std::move(task));

  Step();
}

void AgentRunner::Step() {
  if (state_ != mojom::RunState::kRunning)
    return;

  if (ShouldStop()) {
    Finish(mojom::RunState::kFailed,
           "Stopped: the task stopped making progress.");
    return;
  }

  CompletionRequest request;
  request.model = spec_->model->model;
  request.max_output_tokens = spec_->model->max_output_tokens;
  request.messages = history_;
  // Only tools within the task's declared scope are ever shown to the model.
  // A model that cannot see a send tool does not spend turns trying to use it.
  request.tools = tools_->DefinitionsForScope(spec_->write_scope);
  request.system_prompt =
      "You are operating a real web browser on behalf of the user.\n"
      "Call read_page before acting on a page, and act on node ids from the "
      "most recent snapshot - never guess a selector or an id.\n"
      "After any action that triggers loading, wait for the result rather than "
      "assuming it succeeded.\n"
      "If you cannot determine something, say so instead of inventing it.\n"
      "Stop when the task is done and state what you produced.";

  provider_->Complete(
      std::move(request),
      base::BindOnce(&AgentRunner::OnCompletion, weak_factory_.GetWeakPtr()));
}

void AgentRunner::OnCompletion(CompletionResponse response) {
  if (state_ != mojom::RunState::kRunning)
    return;

  if (!response.error.empty()) {
    // Fail over to the other provider once, for errors that could plausibly
    // succeed elsewhere. A malformed request will fail identically on both.
    if (response.retryable && failover_) {
      provider_ = std::move(failover_);
      failover_.reset();
      Step();
      return;
    }
    Finish(mojom::RunState::kFailed, response.error);
    return;
  }

  if (!ChargeAndCheckBudget(response.input_tokens, response.output_tokens)) {
    Finish(mojom::RunState::kFailed,
           "Stopped: the task reached its credit budget.");
    return;
  }

  Message assistant;
  assistant.role = Message::Role::kAssistant;
  assistant.text = response.text;
  assistant.tool_calls = response.tool_calls;
  history_.push_back(assistant);

  auto progress = mojom::RunProgress::New();
  progress->run_id = run_id_;
  progress->state = state_;
  progress->actions_taken = static_cast<uint32_t>(actions_.size());
  progress->input_tokens = response.input_tokens;
  progress->output_tokens = response.output_tokens;
  progress->credits_spent = credits_spent_;
  if (!response.text.empty())
    progress->current_step = response.text;
  delegate_->OnProgress(*progress);

  // No tool calls means the model considers the task finished.
  if (response.tool_calls.empty()) {
    Finish(mojom::RunState::kSucceeded, response.text);
    return;
  }

  ExecuteToolCalls(std::move(response.tool_calls));
}

void AgentRunner::ExecuteToolCalls(std::vector<ToolCall> calls) {
  if (calls.empty()) {
    Step();
    return;
  }

  ToolCall call = std::move(calls.front());
  calls.erase(calls.begin());
  pending_calls_ = std::move(calls);

  Tool* tool = tools_->Get(call.name);
  if (!tool) {
    ToolResult result;
    result.tool_call_id = call.id;
    result.content = base::StrCat({"No such tool: ", call.name});
    result.is_error = true;
    OnToolFinished(std::move(result));
    return;
  }

  recent_action_digests_.push_back(DigestOf(call));
  if (recent_action_digests_.size() > kRepeatWindow)
    recent_action_digests_.erase(recent_action_digests_.begin());

  if (RequiresApproval(call)) {
    // The run blocks here until ResolveApproval. Nothing proceeds in the
    // meantime, so the failure mode of an unanswered prompt is a stalled task
    // rather than an unintended action.
    state_ = mojom::RunState::kAwaitingApproval;
    pending_call_ = std::move(call);

    auto request = mojom::ApprovalRequest::New();
    request->run_id = run_id_;
    request->tool_name = pending_call_->name;
    request->rationale = history_.empty() ? "" : history_.back().text;
    request->effect_summary = tool->DescribeEffect(pending_call_->input);
    std::string payload;
    base::JSONWriter::WriteWithOptions(
        pending_call_->input, base::JSONWriter::OPTIONS_PRETTY_PRINT, &payload);
    request->payload_preview = payload;
    delegate_->OnApprovalRequired(*request);
    return;
  }

  DispatchTool(std::move(call));
}

void AgentRunner::DispatchTool(ToolCall call) {
  Tool* tool = tools_->Get(call.name);
  ToolContext context;
  context.web_contents = web_contents_;
  context.page = page_.get();
  context.run_id = run_id_;
  context.scope = spec_->write_scope;

  const std::string call_id = call.id;
  tool->Run(context, std::move(call.input),
            base::BindOnce(
                [](base::WeakPtr<AgentRunner> self, std::string id,
                   std::string name, ToolResult result) {
                  if (!self)
                    return;
                  result.tool_call_id = id;
                  self->RecordAction(name, result);
                  self->OnToolFinished(std::move(result));
                },
                weak_factory_.GetWeakPtr(), call_id, call.name));
}

void AgentRunner::RecordAction(const std::string& tool_name,
                               const ToolResult& result) {
  auto action = mojom::ActionRecord::New();
  action->tool_name = tool_name;
  action->summary = result.is_error
                        ? base::StrCat({tool_name, " failed"})
                        : base::StrCat({tool_name, " ok"});
  action->succeeded = !result.is_error;
  if (result.is_error)
    action->error = result.content;
  action->was_approved = approved_last_call_;
  approved_last_call_ = false;

  actions_.push_back(action->Clone());
  delegate_->OnAction(*action);
}

void AgentRunner::OnToolFinished(ToolResult result) {
  consecutive_failures_ = result.is_error ? consecutive_failures_ + 1 : 0;

  Message tool_turn;
  tool_turn.role = Message::Role::kUser;
  tool_turn.tool_results.push_back(std::move(result));
  history_.push_back(std::move(tool_turn));

  // Drain any remaining calls from the same assistant turn before stepping.
  if (!pending_calls_.empty()) {
    std::vector<ToolCall> next = std::move(pending_calls_);
    pending_calls_.clear();
    ExecuteToolCalls(std::move(next));
    return;
  }

  Step();
}

bool AgentRunner::RequiresApproval(const ToolCall& call) const {
  Tool* tool = tools_->Get(call.name);
  if (!tool)
    return false;

  if (ScopeRank(tool->RequiredScope()) > ScopeRank(spec_->write_scope))
    return true;

  // A click is read-only in itself, but clicking a submit control is not.
  // Without this, "click the Send button" would slip through a read-only task
  // as an ordinary click - the obvious hole in scope-by-tool-name alone.
  if (call.name == "click" && page_) {
    std::optional<int> node_id = call.input.FindInt("node_id");
    if (node_id && page_->IsSubmitLike(*node_id))
      return ScopeRank(mojom::WriteScope::kSend) >
             ScopeRank(spec_->write_scope);
  }
  return false;
}

void AgentRunner::ResolveApproval(bool approved,
                                  const std::string& user_note) {
  if (state_ != mojom::RunState::kAwaitingApproval || !pending_call_)
    return;

  state_ = mojom::RunState::kRunning;
  ToolCall call = std::move(*pending_call_);
  pending_call_.reset();

  if (!approved) {
    ToolResult result;
    result.tool_call_id = call.id;
    result.content = user_note.empty()
        ? "The user declined this action. Do not retry it; choose a different "
          "approach or stop and explain what you cannot do."
        : base::StrCat({"The user declined this action, saying: ", user_note});
    result.is_error = true;
    RecordAction(call.name, result);
    OnToolFinished(std::move(result));
    return;
  }

  approved_last_call_ = true;
  if (!user_note.empty()) {
    Message note;
    note.role = Message::Role::kUser;
    note.text = base::StrCat({"The user approved, with a note: ", user_note});
    history_.push_back(std::move(note));
  }
  DispatchTool(std::move(call));
}

bool AgentRunner::ChargeAndCheckBudget(uint32_t input_tokens,
                                       uint32_t output_tokens) {
  const double cost =
      (input_tokens / 1e6) * provider_->InputCostPerMillion(spec_->model->model) +
      (output_tokens / 1e6) * provider_->OutputCostPerMillion(spec_->model->model);
  // Credits are thousandths of a cent, so integer arithmetic throughout.
  credits_spent_ += static_cast<uint64_t>(cost * 100000.0);
  return credits_spent_ < spec_->credit_budget;
}

bool AgentRunner::ShouldStop() const {
  if (consecutive_failures_ >= kMaxConsecutiveFailures)
    return true;
  if (actions_.size() >= kMaxActions)
    return true;

  // Repeating the same call with the same inputs is the signature failure of
  // long-running browser agents: the page is not in the state the model
  // believes, so it retries forever.
  if (recent_action_digests_.size() >= kRepeatWindow) {
    for (const std::string& digest : recent_action_digests_) {
      const auto count = std::count(recent_action_digests_.begin(),
                                    recent_action_digests_.end(), digest);
      if (count >= kMaxRepeatsInWindow)
        return true;
    }
  }
  return false;
}

void AgentRunner::Cancel() {
  if (state_ == mojom::RunState::kSucceeded ||
      state_ == mojom::RunState::kFailed) {
    return;
  }
  Finish(mojom::RunState::kCancelled, "Cancelled by the user.");
}

void AgentRunner::Pause() {
  if (state_ == mojom::RunState::kRunning)
    state_ = mojom::RunState::kPaused;
}

void AgentRunner::Resume() {
  if (state_ == mojom::RunState::kPaused) {
    state_ = mojom::RunState::kRunning;
    Step();
  }
}

void AgentRunner::Finish(mojom::RunState state, const std::string& summary) {
  state_ = state;
  weak_factory_.InvalidateWeakPtrs();
  delegate_->OnFinished(state, summary);
}

}  // namespace flux
