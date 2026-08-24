// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_
#define CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/flux/agent/agent_tab.h"
#include "chrome/browser/flux/agent/page_context.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/providers/llm_provider.h"

class Profile;

namespace flux {

// Executes one task: the model proposes tool calls, the runner performs them
// against a real page, feeds results back, and repeats until the task is done
// or a stop condition fires.
//
// Every run gets its own tab, but NOT its own profile. Isolating the profile
// was the original design and it was wrong for this product: the reason to put
// an agent inside the browser at all is that the user is already signed in
// everywhere, and a run in a fresh profile is signed in nowhere. It shares the
// user's cookie jar, which is what makes "check my inbox" a task rather than a
// login problem.
//
// The cost of that is real and is accepted: concurrent runs touching the same
// service share its session, so they can collide over a CSRF token or a
// single-session service. The concurrency cap and the write scopes are what
// keep that survivable.
class AgentRunner {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnProgress(const mojom::RunProgress& progress) = 0;
    // The run id is passed explicitly. ActionRecord does not carry one, and
    // the service used to recover it by scanning for the first runner in a
    // running state - which is the wrong runner the moment two runs overlap,
    // and subagents make that the normal case rather than the rare one.
    virtual void OnAction(const std::string& run_id,
                          const mojom::ActionRecord& action) = 0;
    // The runner blocks here until ResolveApproval() is called.
    virtual void OnApprovalRequired(const mojom::ApprovalRequest& request) = 0;
    // The runner blocks here until ResolveQuestions() is called.
    virtual void OnQuestionsAsked(const mojom::QuestionRequest& request) = 0;
    virtual void OnFinished(const std::string& run_id,
                            mojom::RunState state,
                            const std::string& summary) = 0;
  };

  AgentRunner(std::string run_id,
              Profile* profile,
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

  // Poses questions to the user and blocks until they come back. `callback`
  // is the tool call's own result callback, held until then.
  void AskUser(std::vector<mojom::AgentQuestionPtr> questions,
               const std::string& preamble,
               base::OnceCallback<void(std::vector<mojom::QuestionAnswerPtr>)>
                   answered);
  void ResolveQuestions(std::vector<mojom::QuestionAnswerPtr> answers);

  // A message typed into the run's composer while it is going. Appended to the
  // history and picked up on the next turn rather than interrupting the one in
  // flight: cutting off a tool call mid-execution to read a new instruction is
  // how a half-finished write happens.
  void AddUserMessage(const std::string& text);

  const std::string& run_id() const { return run_id_; }
  mojom::RunState state() const { return state_; }

  // The spec a run was started with. Subagents inherit their parent's scope,
  // model and profile from it, so a child can never be given more authority
  // than the run that spawned it.
  const mojom::TaskSpec* spec() const { return spec_.get(); }

 private:
  void Step();                                   // one turn of the loop
  void OnCompletion(CompletionResponse response);

  // Runs calls from one assistant turn in order, pausing for approval where
  // the scope requires it. Remaining calls are held in `pending_calls_`.
  void ExecuteToolCalls(std::vector<ToolCall> calls);
  void DispatchTool(ToolCall call);
  // `effect` is the tool's own DescribeEffect() for this call, captured before
  // the input was moved into Run(). Without it the transcript reads "click ok"
  // for every step, which is a list of tool names rather than a record of what
  // happened.
  void RecordAction(const std::string& tool_name,
                    const std::string& effect,
                    base::TimeTicks started_at,
                    const ToolResult& result);
  void OnToolFinished(ToolResult result);

  // Opens the run's tab if it does not have one yet. False if a tab could not
  // be opened, which fails the tool call rather than the run.
  bool EnsurePage();
  void Finish(mojom::RunState state, const std::string& summary);

  // The user closed the run's tab out from under it.
  void OnTabClosed();

  // Returns true when `call` exceeds the task's declared WriteScope and must
  // be approved by a human first. This is the enforcement point that makes
  // WriteScope structural rather than advisory.
  bool RequiresApproval(const ToolCall& call) const;

  // Charges the run before dispatching. Fails closed on budget exhaustion
  // rather than discovering the overrun after the fact.
  bool ChargeAndCheckBudget(uint32_t input_tokens, uint32_t output_tokens);

  // Stop conditions. Without these an agent loops until the budget is gone.
  // The URL the next tool call will act against, or empty before the agent's
  // tab exists. Part of a call's identity for the stall detector.
  std::string CurrentPageURL() const;

  // Why the run should stop, or empty if it should not. Spelled out rather
  // than a bool so both the user and the log learn which rule fired.
  std::string StopReason() const;
  bool ShouldStop() const;

  const std::string run_id_;
  raw_ptr<Profile> profile_;
  mojom::TaskSpecPtr spec_;
  std::unique_ptr<LLMProvider> provider_;
  std::unique_ptr<LLMProvider> failover_;
  raw_ptr<ToolRegistry> tools_;
  raw_ptr<Delegate> delegate_;

  std::vector<Message> history_;
  std::vector<mojom::ActionRecordPtr> actions_;
  // The run's own tab, in the user's profile. Opened on Start() - a run with
  // no page to act on can do nothing, and every browser tool was previously
  // handed a null WebContents because nothing ever created one.
  std::unique_ptr<AgentTab> tab_;
  std::unique_ptr<PageContext> page_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Remaining calls from the current assistant turn, and the one held while
  // an approval prompt is open.
  std::vector<ToolCall> pending_calls_;
  std::optional<ToolCall> pending_call_;
  bool approved_last_call_ = false;

  // Held while the run is kAwaitingInput. The state before asking is restored
  // on the way out: a question posed from inside a tool call has to return to
  // the middle of that tool call, not to the top of the loop.
  base::OnceCallback<void(std::vector<mojom::QuestionAnswerPtr>)>
      pending_answers_;
  mojom::RunState state_before_question_ = mojom::RunState::kRunning;

  mojom::RunState state_ = mojom::RunState::kQueued;
  uint32_t consecutive_failures_ = 0;
  uint64_t credits_spent_ = 0;
  // When the current model call went out, so the transcript can say "Thought
  // for 8s" rather than showing an unexplained gap.
  base::TimeTicks turn_started_at_;

  // Detects the agent repeating an action that is not making progress — the
  // most common failure mode in long-running browser agents.
  std::vector<std::string> recent_action_digests_;

  base::WeakPtrFactory<AgentRunner> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_AGENT_RUNNER_H_
