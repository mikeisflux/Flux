// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_ASK_SESSION_H_
#define CHROME_BROWSER_FLUX_AGENT_ASK_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/providers/llm_provider.h"

class Profile;

namespace flux {

class WorkflowScheduler;

// The conversation behind the Ask Flux panel.
//
// Deliberately not an AgentRunner. A run browses: it owns a tab, a page
// context, a write scope, a credit ceiling and a row in the run list, and it
// ends. This does none of that. It answers questions about Flux and then
// carries them out - "how do I post twice a day on a schedule" followed by
// actually saving that workflow - so its tools act on Flux's own state rather
// than on a web page.
//
// The history lives here, in the browser process, rather than in the panel.
// Closing the panel, reopening it, or reloading its WebUI must not lose the
// conversation, and the approval queue already taught this codebase what
// happens when something the user is mid-way through exists only in a
// document that can be thrown away.
class AskSession {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // A turn was appended or an in-flight one changed. The whole turn goes
    // over, not a delta: it is short, and a panel that redraws from it cannot
    // drift out of step with what is actually here.
    virtual void OnAskTurn(const mojom::AskTurn& turn, bool busy) = 0;
    virtual void OnAskQuestions(const mojom::QuestionRequest& request) = 0;
  };

  AskSession(Profile* profile,
             WorkflowScheduler* scheduler,
             Delegate* delegate);
  AskSession(const AskSession&) = delete;
  AskSession& operator=(const AskSession&) = delete;
  ~AskSession();

  void Send(const std::string& message,
            const std::string& model,
            std::vector<mojom::AskAttachmentPtr> attachments);
  void Answer(std::vector<mojom::QuestionAnswerPtr> answers);
  void Reset();

  std::vector<mojom::AskTurnPtr> Thread() const;
  bool busy() const { return busy_; }
  // What the assistant is waiting to be told, so a panel that has just opened
  // can put the question back on screen. Null when nothing is pending.
  mojom::QuestionRequestPtr PendingQuestion() const;

 private:
  void Step();
  void OnCompletion(CompletionResponse response);

  // The tool calls from one turn, run one after another.
  //
  // Sequential and callback-driven rather than a loop returning values,
  // because reading a file has to happen on a worker thread: blocking IO on
  // the UI thread is a jank bug that Chromium's thread restrictions turn into
  // a DCHECK. Everything else here is synchronous and simply calls back
  // immediately.
  void RunNextTool();
  // Records one result against the turn being assembled and advances. Split
  // from OnToolDone so the synchronous path can record without re-entering
  // RunNextTool: doing both from inside the loop ran the completion block
  // twice - a moved-from results message pushed into history, a duplicate
  // turn on screen, and a second concurrent request to the model.
  void RecordToolResult(ToolResult result);
  void OnToolDone(ToolResult result);
  void RunFileTool(const ToolCall& call,
                   base::OnceCallback<void(ToolResult)> done);
  // Every tool that answers without touching the disk. Synchronous, so its
  // result goes straight to RecordToolResult.
  ToolResult RunTool(const ToolCall& call);
  std::vector<ToolDefinition> Tools() const;
  std::string SystemPrompt() const;
  void Emit(const mojom::AskTurn& turn);

  const raw_ptr<Profile> profile_;
  const raw_ptr<WorkflowScheduler> scheduler_;
  const raw_ptr<Delegate> delegate_;

  std::unique_ptr<LLMProvider> provider_;
  // Which one provider_ is, so a model change that crosses providers rebuilds
  // it rather than sending the name to the wrong API.
  bool provider_is_anthropic_ = true;
  std::string model_;
  std::vector<Message> history_;
  std::vector<mojom::AskTurnPtr> turns_;
  base::TimeTicks turn_started_at_;
  bool busy_ = false;
  // Set while a question is outstanding, so the answer knows which call to
  // resolve.
  std::string pending_question_call_id_;
  mojom::QuestionRequestPtr pending_question_;

  // The turn being assembled while its tool calls run.
  std::vector<ToolCall> pending_calls_;
  size_t next_call_ = 0;
  Message pending_results_;
  mojom::AskTurnPtr pending_turn_;

  base::WeakPtrFactory<AskSession> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_ASK_SESSION_H_
