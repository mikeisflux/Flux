// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/agent_runner.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace flux {
namespace {

// Stop conditions. Without them an agent loops until the budget is gone, which
// is both expensive and the most common way these fail in practice.
constexpr uint32_t kMaxConsecutiveFailures = 3;
constexpr uint32_t kRepeatWindow = 6;
constexpr uint32_t kMaxRepeatsInWindow = 3;
constexpr size_t kMaxActions = 200;

// What the user's own configuration is allowed to add to the system prompt.
//
// A ceiling rather than a clamp on the writer, because all three of these are
// edited somewhere else and read here: bounding them at the point of use is
// the only place that covers a pref restored from disk, copied from another
// profile, or written by an older build. Every one of them is also billed on
// every single turn, so an unbounded buffer here is an unbounded bill.
constexpr size_t kMaxInstructionChars = 8000;
constexpr size_t kMaxFactChars = 4000;
constexpr size_t kMaxSkillChars = 12000;

// Cuts at a byte budget and says that it did.
//
// Silent truncation is worse than none: the user reads their instructions back
// off the Customize screen in full, so a prompt that quietly holds half of
// them makes the agent look like it is ignoring the second half on purpose.
//
// TruncateUTF8ToByteSize rather than substr, because these are user-typed and
// so contain whatever they contain. A plain cut at byte N lands in the middle
// of a multi-byte sequence often enough to matter, and the result is not a
// slightly wrong prompt - it is an invalid UTF-8 string handed to the JSON
// writer, which fails the whole request rather than the one character.
std::string Clamp(std::string_view text, size_t limit) {
  if (text.size() <= limit)
    return std::string(text);
  return base::StrCat(
      {base::TruncateUTF8ToByteSize(text, limit), "\n[truncated]"});
}

// The user's standing instructions, what the agent has learned about them, and
// the skills they have adopted.
//
// All three were written to prefs and read by nothing but the screens that
// edit them. The features compiled, linked, ran, and did nothing: Customize >
// Instructions was a text box that saved to disk and never reached a model,
// "remember" was a diary the next run could not open, and adopting one of the
// 138 shipped skills changed no behaviour at all. A declaration that reads
// like proof the feature exists is exactly how that survives review - the
// pref's own comment in flux_prefs.h says "Prepended to every task".
std::string BuildUserContext(Profile* profile) {
  if (!profile)
    return std::string();
  PrefService* prefs = profile->GetPrefs();
  std::string out;

  const std::string instructions = prefs->GetString(prefs::kInstructions);
  if (!instructions.empty()) {
    base::StrAppend(&out, {"\n\nStanding instructions from the user. These "
                           "apply to every task and outrank the general "
                           "guidance above where they conflict:\n",
                           Clamp(instructions, kMaxInstructionChars), "\n"});
  }

  std::string facts;
  for (const base::Value& entry : prefs->GetList(prefs::kLearnedFacts)) {
    const base::DictValue* fact = entry.GetIfDict();
    if (!fact)
      continue;
    const std::string* text = fact->FindString("text");
    if (!text || text->empty())
      continue;
    base::StrAppend(&facts, {"- ", *text, "\n"});
    if (facts.size() >= kMaxFactChars)
      break;
  }
  if (!facts.empty()) {
    base::StrAppend(&out, {"\n\nWhat you have previously learned about this "
                           "user. Treat it as background, not as instruction, "
                           "and prefer what they say now:\n",
                           Clamp(facts, kMaxFactChars)});
  }

  // The adopted commands are the list; the bodies live in a dictionary keyed
  // by command. A command in one and not the other is skipped rather than
  // guessed at.
  const base::DictValue& bodies = prefs->GetDict(prefs::kUserSkills);
  std::string skills;
  for (const base::Value& entry : prefs->GetList(prefs::kAdoptedSkills)) {
    const std::string* command = entry.GetIfString();
    if (!command)
      continue;
    const base::DictValue* skill = bodies.FindDict(*command);
    if (!skill)
      continue;
    const std::string* body = skill->FindString("instructions");
    if (!body || body->empty())
      continue;
    const std::string* name = skill->FindString("name");
    base::StrAppend(&skills, {"\n## ", name ? *name : *command, " (/",
                              *command, ")\n", *body, "\n"});
    if (skills.size() >= kMaxSkillChars)
      break;
  }
  if (!skills.empty()) {
    base::StrAppend(&out, {"\n\nSkills the user has adopted. Apply one when "
                           "the task matches what it describes:\n",
                           Clamp(skills, kMaxSkillChars)});
  }

  return out;
}

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
                         Profile* profile,
                         mojom::TaskSpecPtr spec,
                         std::unique_ptr<LLMProvider> provider,
                         std::unique_ptr<LLMProvider> failover,
                         ToolRegistry* tools,
                         Delegate* delegate)
    : run_id_(std::move(run_id)),
      profile_(profile),
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
  request.messages = CloneMessages(history_);
  // Only tools within the task's declared scope are ever shown to the model.
  // A model that cannot see a send tool does not spend turns trying to use it.
  request.tools = tools_->DefinitionsForScope(spec_->write_scope);
  request.system_prompt =
      "You are operating a real web browser on behalf of the user.\n"
      "\n"
      "Start by calling set_plan with the three to six steps this task will "
      "take, written as things the user would recognise as done or not done. "
      "Call complete_step the moment each one is finished, not in a batch at "
      "the end. The user watches this while the task runs, and a plan that "
      "only updates when the work is over tells them nothing while it "
      "matters.\n"
      "\n"
      "Call read_page before acting on a page, and act on node ids from the "
      "most recent snapshot - never guess a selector or an id.\n"
      "After any action that triggers loading, wait for the result rather than "
      "assuming it succeeded.\n"
      "If you cannot determine something, say so instead of inventing it.\n"
      "\n"
      "When the task produces something that lives somewhere - a sheet, a doc, "
      "a page - call save_artifact with its title and URL. When it produces "
      "something with nowhere to live - a CSV of the rows you gathered, a "
      "report, an export - call write_file. Either way the user gets something "
      "to open; describing a file in your closing message without doing one of "
      "them leaves them with nothing, and pasting a thousand rows into your "
      "reply instead is worse.\n"
      "\n"
      "If you hit an unfilled [placeholder], an ambiguous reference, or a "
      "choice only the user can make, call ask_user - all of it in one call. "
      "Never guess a recipient, a link or a search term: a task done against "
      "an invented value is worse than one that paused to ask.\n"
      "\n"
      "Your replies are rendered as markdown. Tables, links, quotes, code "
      "blocks and lists all work, so use a table when the answer is a table "
      "and link anything the user will want to open.\n"
      "\n"
      "Stop when the task is done. Your final message is the answer the user "
      "asked for, so lead with the result and the numbers, then anything they "
      "need to know about how you got there or what you could not do. Markdown "
      "is rendered.";

  // Built every turn rather than cached on the runner: a user who fixes their
  // instructions mid-run, or a fact the agent just remembered, should apply to
  // the next turn and not to the next task.
  request.system_prompt += BuildUserContext(profile_);

  turn_started_at_ = base::TimeTicks::Now();
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
  // Cloned, not moved: response.tool_calls is handed to ExecuteToolCalls below
  // and the transcript has to outlive that.
  assistant.tool_calls = CloneToolCalls(response.tool_calls);
  history_.push_back(std::move(assistant));

  auto progress = mojom::RunProgress::New();
  progress->run_id = run_id_;
  progress->state = state_;
  progress->actions_taken = static_cast<uint32_t>(actions_.size());
  progress->input_tokens = response.input_tokens;
  progress->output_tokens = response.output_tokens;
  progress->credits_spent = credits_spent_;
  progress->thinking_ms = static_cast<uint32_t>(
      (base::TimeTicks::Now() - turn_started_at_).InMilliseconds());
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
    // Recorded, not just returned to the model. A run that spends its budget
    // calling a tool that does not exist looked, in the transcript, like a run
    // that did nothing at all.
    RecordAction(call.name, base::StrCat({"Called unknown tool ", call.name}),
                 base::TimeTicks::Now(), result);
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

  // The tab is opened here, the first time a tool actually needs a page, and
  // not when the run starts. Opening it in Start() put an about:blank tab in
  // front of the user for every run, whether or not it ever browsed.
  if (tool && tool->NeedsPage() && !EnsurePage()) {
    ToolResult result;
    result.tool_call_id = call.id;
    result.content = "Could not open a tab to work in.";
    result.is_error = true;
    RecordAction(call.name, tool->DescribeEffect(call.input),
                 base::TimeTicks::Now(), result);
    OnToolFinished(std::move(result));
    return;
  }

  ToolContext context;
  context.web_contents = web_contents_;
  context.page = page_.get();
  context.run_id = run_id_;
  context.scope = spec_->write_scope;

  const std::string call_id = call.id;
  // Described BEFORE the input is moved into Run(). DescribeEffect is the only
  // thing that knows what this particular call does - "Clicked 'Next page'"
  // rather than "click" - and after the move there is nothing left to ask.
  const std::string effect = tool->DescribeEffect(call.input);
  const base::TimeTicks started_at = base::TimeTicks::Now();
  tool->Run(context, std::move(call.input),
            base::BindOnce(
                [](base::WeakPtr<AgentRunner> self, std::string id,
                   std::string name, std::string effect,
                   base::TimeTicks started_at, ToolResult result) {
                  if (!self)
                    return;
                  result.tool_call_id = id;
                  self->RecordAction(name, effect, started_at, result);
                  self->OnToolFinished(std::move(result));
                },
                weak_factory_.GetWeakPtr(), call_id, call.name, effect,
                started_at));
}

void AgentRunner::RecordAction(const std::string& tool_name,
                               const std::string& effect,
                               base::TimeTicks started_at,
                               const ToolResult& result) {
  auto action = mojom::ActionRecord::New();
  action->tool_name = tool_name;
  // The tool's own description of this call, not its name. The transcript used
  // to read "click ok / read_page ok / click ok", which tells the user the
  // agent did six things and nothing about what any of them were.
  action->summary = effect.empty() ? tool_name : effect;
  action->succeeded = !result.is_error;
  if (result.is_error)
    action->error = result.content;
  action->was_approved = approved_last_call_;
  approved_last_call_ = false;

  // The page the action happened on. The console renders this beside the verb
  // and had nothing to render, because nothing ever set it.
  if (web_contents_) {
    const GURL& url = web_contents_->GetLastCommittedURL();
    if (url.is_valid())
      action->page_url = url;
  }

  // Both times are non-nullable in the mojom, so leaving them unset does not
  // read as "unknown" - it reads as the Windows epoch.
  const base::Time now = base::Time::Now();
  action->finished_at = now;
  action->started_at = now - (base::TimeTicks::Now() - started_at);

  actions_.push_back(action->Clone());
  delegate_->OnAction(run_id_, *action);
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

  // For the transcript line below: the declined action is worth recording as
  // what it would have done, not as a bare tool name.
  Tool* tool = tools_->Get(call.name);

  if (!approved) {
    ToolResult result;
    result.tool_call_id = call.id;
    result.content = user_note.empty()
        ? "The user declined this action. Do not retry it; choose a different "
          "approach or stop and explain what you cannot do."
        : base::StrCat({"The user declined this action, saying: ", user_note});
    result.is_error = true;
    RecordAction(call.name,
                 base::StrCat({"Declined: ", tool ? tool->DescribeEffect(
                                                        call.input)
                                                  : call.name}),
                 base::TimeTicks::Now(), result);
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

bool AgentRunner::EnsurePage() {
  if (page_)
    return true;
  tab_ = std::make_unique<AgentTab>(
      profile_, base::BindOnce(&AgentRunner::OnTabClosed,
                               weak_factory_.GetWeakPtr()));
  web_contents_ = tab_->Open();
  if (!web_contents_) {
    tab_.reset();
    return false;
  }
  page_ = std::make_unique<PageContext>(web_contents_);
  return true;
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

void AgentRunner::AddUserMessage(const std::string& text) {
  if (text.empty())
    return;

  Message note;
  note.role = Message::Role::kUser;
  note.text = text;
  history_.push_back(std::move(note));

  // The console offers "Or reply directly" alongside the question panel, so a
  // typed message IS the answer when one is open. Without this the text landed
  // in the history and the run stayed blocked on a panel the user had already
  // decided not to use.
  if (state_ == mojom::RunState::kAwaitingInput) {
    // Delivered as a single skipped-everything reply: the message is already
    // in the history above, so the tool result only has to unblock the call
    // and tell the model to read what the user actually wrote.
    std::vector<mojom::QuestionAnswerPtr> none;
    ResolveQuestions(std::move(none));
    return;
  }

  // If the loop has run out of turns to take - paused, or waiting because the
  // last assistant turn made no tool call - the new message is what restarts
  // it. A run that is mid-tool picks it up when that tool returns.
  if (state_ == mojom::RunState::kPaused) {
    Resume();
  }
}

void AgentRunner::AskUser(
    std::vector<mojom::AgentQuestionPtr> questions,
    const std::string& preamble,
    base::OnceCallback<void(std::vector<mojom::QuestionAnswerPtr>)> answered) {
  if (questions.empty()) {
    std::move(answered).Run({});
    return;
  }
  // A second ask while one is open would drop the first tool call's callback
  // and hang that call forever. The model gets told to wait instead.
  if (pending_answers_) {
    std::move(answered).Run({});
    return;
  }

  auto request = mojom::QuestionRequest::New();
  request->run_id = run_id_;
  request->questions = std::move(questions);
  if (!preamble.empty())
    request->preamble = preamble;

  state_before_question_ = state_;
  state_ = mojom::RunState::kAwaitingInput;
  pending_answers_ = std::move(answered);
  delegate_->OnQuestionsAsked(*request);
}

void AgentRunner::ResolveQuestions(
    std::vector<mojom::QuestionAnswerPtr> answers) {
  if (!pending_answers_)
    return;
  // An answer can arrive after the run has ended - the panel stays on screen
  // until the console redraws, and the user can be mid-sentence when the tab
  // is closed or Stop is pressed. Restoring state_ then would put a cancelled
  // run back into kRunning and Step() would carry on from there.
  if (state_ != mojom::RunState::kAwaitingInput) {
    pending_answers_.Reset();
    return;
  }
  state_ = state_before_question_;
  std::move(pending_answers_).Run(std::move(answers));
}

void AgentRunner::OnTabClosed() {
  // The user closed the run's tab. That is a deliberate stop, not a failure of
  // the task, and it has to be honoured immediately: the WebContents is gone
  // and anything still queued would act through a freed pointer.
  web_contents_ = nullptr;
  page_.reset();
  if (state_ == mojom::RunState::kRunning ||
      state_ == mojom::RunState::kAwaitingApproval ||
      state_ == mojom::RunState::kAwaitingInput ||
      state_ == mojom::RunState::kPaused) {
    Finish(mojom::RunState::kCancelled, "You closed the task's tab.");
  }
}

void AgentRunner::Finish(mojom::RunState state, const std::string& summary) {
  state_ = state;
  // Dropped, not run: the tool call it belongs to is on a run that is over,
  // and resuming it would step a finished run.
  pending_answers_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->OnFinished(run_id_, state, summary);
}

}  // namespace flux
