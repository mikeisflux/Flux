// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_
#define CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/agent/page_context.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/providers/llm_provider.h"

namespace flux {

// Executes one task: the model proposes tool calls, the runner performs them
// against a real page, feeds results back, and repeats until the task is done
// or a stop condition fires.
//
// Every run owns its own WebContents in its own profile, so ten concurrent
// runs cannot collide over cookies, CSRF tokens or single-session services.
class AgentRunner {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnProgress(const mojom::RunProgress& progress) = 0;
    virtual void OnAction(const mojom::ActionRecord& action) = 0;
    // The runner blocks here until ResolveApproval() is called.
    virtual void OnApprovalRequired(const mojom::ApprovalRequest& request) = 0;
    virtual void OnFinished(mojom::RunState state,
                            const std::string& summary) = 0;
  };

  AgentRunner(std::string run_id,
              mojom::TaskSpecPtr spec,
              std::unique_ptr<LLMProvider> provider,
              std::unique_ptr<LLMProvider> failover,  // may be null
              ToolRegistry* tools,
              Delegate* delegate);
  ~AgentRunner();

  AgentRunner(const AgentRunner&) = delete;
  AgentRunner& operator=(const AgentRunner&) = delete;

  void Start();
  void Cancel();
  void Pause();
  void Resume();
  void ResolveApproval(bool approved, const std::string& user_note);

  // A message typed into the run's composer while it is going. Appended to the
  // history and picked up on the next turn rather than interrupting the one in
  // flight: cutting off a tool call mid-execution to read a new instruction is
  // how a half-finished write happens.
  void AddUserMessage(const std::string& text);

  const std::string& run_id() const { return run_id_; }
  mojom::RunState state() const { return state_; }

 private:
  void Step();                                   // one turn of the loop
  void OnCompletion(CompletionResponse response);

  // Runs calls from one assistant turn in order, pausing for approval where
  // the scope requires it. Remaining calls are held in `pending_calls_`.
  void ExecuteToolCalls(std::vector<ToolCall> calls);
  void DispatchTool(ToolCall call);
  void RecordAction(const std::string& tool_name, const ToolResult& result);
  void OnToolFinished(ToolResult result);
  void Finish(mojom::RunState state, const std::string& summary);

  // Returns true when `call` exceeds the task's declared WriteScope and must
  // be approved by a human first. This is the enforcement point that makes
  // WriteScope structural rather than advisory.
  bool RequiresApproval(const ToolCall& call) const;

  // Charges the run before dispatching. Fails closed on budget exhaustion
  // rather than discovering the overrun after the fact.
  bool ChargeAndCheckBudget(uint32_t input_tokens, uint32_t output_tokens);

  // Stop conditions. Without these an agent loops until the budget is gone.
  bool ShouldStop() const;

  const std::string run_id_;
  mojom::TaskSpecPtr spec_;
  std::unique_ptr<LLMProvider> provider_;
  std::unique_ptr<LLMProvider> failover_;
  raw_ptr<ToolRegistry> tools_;
  raw_ptr<Delegate> delegate_;

  std::vector<Message> history_;
  std::vector<mojom::ActionRecordPtr> actions_;
  std::unique_ptr<PageContext> page_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Remaining calls from the current assistant turn, and the one held while
  // an approval prompt is open.
  std::vector<ToolCall> pending_calls_;
  std::optional<ToolCall> pending_call_;
  bool approved_last_call_ = false;

  mojom::RunState state_ = mojom::RunState::kQueued;
  uint32_t consecutive_failures_ = 0;
  uint64_t credits_spent_ = 0;

  // Detects the agent repeating an action that is not making progress — the
  // most common failure mode in long-running browser agents.
  std::vector<std::string> recent_action_digests_;

  base::WeakPtrFactory<AgentRunner> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_
