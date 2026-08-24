// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/ask_session.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/flux/providers/anthropic_provider.h"
#include "chrome/browser/flux/providers/openai_provider.h"
#include "chrome/browser/flux/scheduler/workflow_scheduler.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flux {
namespace {

// A conversation is cheap next to a run, but not free, and it has no credit
// ceiling of its own because it is not a task the user budgeted. This is the
// backstop against a thread that grows until every turn re-sends an hour of
// history.
constexpr size_t kMaxTurns = 60;

// What an attachment may contribute. The bytes are inlined into the prompt, so
// this is a prompt-size limit rather than a file-size one.
constexpr size_t kMaxAttachmentChars = 20000;

// How much of a file the model may see, and how many entries a listing may
// return. Both are prompt-size limits rather than filesystem ones.
constexpr size_t kMaxFileChars = 40000;
constexpr size_t kMaxListedEntries = 200;

// The only places the assistant may look.
//
// Three fixed directories rather than a pref, because a pref no screen can
// edit is a setting that does not exist - and a "grant a folder" flow is a
// feature to design, not a default to guess at. These three are where the
// things a person would want a template built from actually live.
std::vector<base::FilePath> FileRoots() {
  std::vector<base::FilePath> roots;
  // Spelled as ints rather than a braced list of the enumerators: the first
  // two are chrome_paths.h's unnamed enum and the third is base::BasePathKey,
  // and a braced list deduces one element type for all of them.
  static constexpr int kRootKeys[] = {chrome::DIR_USER_DOCUMENTS,
                                      chrome::DIR_DEFAULT_DOWNLOADS_SAFE,
                                      base::DIR_USER_DESKTOP};
  for (int key : kRootKeys) {
    base::FilePath dir;
    if (base::PathService::Get(key, &dir) && !dir.empty())
      roots.push_back(dir.StripTrailingSeparators());
  }
  return roots;
}

// Resolves `raw` and returns it only if it lands inside a root.
//
// Resolved FIRST and checked second, and that order is the whole point:
// MakeAbsoluteFilePath expands symlinks, so a link inside Documents pointing
// at C:\Users\Mike\.ssh is caught here. Checking the string before resolving
// would pass it. Blocking, so this only ever runs on a worker.
std::optional<base::FilePath> ResolveInsideRoot(const std::string& raw) {
  if (raw.empty())
    return std::nullopt;
  const base::FilePath resolved =
      base::MakeAbsoluteFilePath(base::FilePath::FromUTF8Unsafe(raw));
  if (resolved.empty() || resolved.ReferencesParent())
    return std::nullopt;
  for (const base::FilePath& root : FileRoots()) {
    if (root == resolved || root.IsParent(resolved))
      return resolved;
  }
  return std::nullopt;
}

// Runs on a worker. Returns the text to hand the model, and whether it failed.
std::pair<std::string, bool> ReadOnWorker(std::string path) {
  const std::optional<base::FilePath> file = ResolveInsideRoot(path);
  if (!file) {
    return {"That path is outside the folders Flux may read (Documents, "
            "Downloads and Desktop).", true};
  }
  std::string body;
  if (!base::ReadFileToStringWithMaxSize(*file, &body, kMaxFileChars)) {
    // A partial read on an oversized file still fills `body`, which is more
    // use than an error - say it was cut rather than refusing outright.
    if (body.empty())
      return {"Could not read that file.", true};
    body += "\n[truncated]";
  }
  return {body, false};
}

std::pair<std::string, bool> ListOnWorker(std::string path) {
  const std::optional<base::FilePath> dir = ResolveInsideRoot(path);
  if (!dir) {
    return {"That path is outside the folders Flux may read (Documents, "
            "Downloads and Desktop).", true};
  }
  std::string out;
  size_t count = 0;
  base::FileEnumerator files(
      *dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath entry = files.Next(); !entry.empty();
       entry = files.Next()) {
    const base::FileEnumerator::FileInfo info = files.GetInfo();
    base::StrAppend(&out, {info.IsDirectory() ? "dir  " : "file ",
                           entry.BaseName().AsUTF8Unsafe(), "\n"});
    if (++count >= kMaxListedEntries) {
      base::StrAppend(&out, {"[more entries not listed]\n"});
      break;
    }
  }
  return {out.empty() ? "(empty)" : out, false};
}

base::DictValue StringProp(const std::string& description) {
  base::DictValue prop;
  prop.Set("type", "string");
  prop.Set("description", description);
  return prop;
}

}  // namespace

AskSession::AskSession(Profile* profile,
                       WorkflowScheduler* scheduler,
                       Delegate* delegate)
    : profile_(profile), scheduler_(scheduler), delegate_(delegate) {}

AskSession::~AskSession() = default;

std::vector<mojom::AskTurnPtr> AskSession::Thread() const {
  std::vector<mojom::AskTurnPtr> out;
  for (const mojom::AskTurnPtr& turn : turns_)
    out.push_back(turn->Clone());
  return out;
}

void AskSession::Reset() {
  history_.clear();
  turns_.clear();
  pending_question_call_id_.clear();
  pending_question_.reset();
  pending_calls_.clear();
  pending_turn_.reset();
  pending_results_ = Message();
  next_call_ = 0;
  busy_ = false;
  provider_.reset();
  // A model request or a file read may still be in flight. Invalidating here
  // is what stops its reply landing in the thread the user just cleared.
  weak_factory_.InvalidateWeakPtrs();
}

void AskSession::Send(const std::string& message,
                      const std::string& model,
                      std::vector<mojom::AskAttachmentPtr> attachments) {
  if (busy_ || message.empty())
    return;

  // "Or reply directly" is the composer under an open question, so a message
  // typed there is the answer to it. Appending it as a new user turn instead
  // would leave the assistant's tool_use with no tool_result after it, which
  // is not a degraded conversation - both providers reject the request.
  if (!pending_question_call_id_.empty()) {
    auto shown = mojom::AskTurn::New();
    shown->from_user = true;
    shown->text = message;
    turns_.push_back(shown->Clone());
    Emit(*shown);

    std::vector<mojom::QuestionAnswerPtr> answers;
    auto answer = mojom::QuestionAnswer::New();
    if (pending_question_ && !pending_question_->questions.empty())
      answer->id = pending_question_->questions.front()->id;
    answer->text = message;
    answers.push_back(std::move(answer));
    Answer(std::move(answers));
    return;
  }

  std::string text = message;
  // Inlined rather than uploaded: neither provider is given a file endpoint
  // here, and a pasted spreadsheet is the case this exists for. Anything that
  // is not text is named and its size reported, because "I attached a PDF" is
  // still worth the model knowing even when it cannot read the bytes.
  for (const mojom::AskAttachmentPtr& file : attachments) {
    if (!file)
      continue;
    const bool textual = file->mime_type.starts_with("text/") ||
                         file->mime_type == "application/json" ||
                         file->mime_type == "text/csv";
    if (textual) {
      std::string body(reinterpret_cast<const char*>(file->bytes.data()),
                       file->bytes.size());
      if (body.size() > kMaxAttachmentChars)
        body = body.substr(0, kMaxAttachmentChars) + "\n[truncated]";
      base::StrAppend(&text, {"\n\n--- ", file->name, " ---\n", body});
    } else {
      base::StrAppend(&text,
                      {"\n\n[attached ", file->name, ", ", file->mime_type,
                       ", ", base::NumberToString(file->bytes.size()),
                       " bytes - not readable as text]"});
    }
  }

  model_ = model;

  Message turn;
  turn.role = Message::Role::kUser;
  turn.text = text;
  history_.push_back(std::move(turn));

  auto shown = mojom::AskTurn::New();
  shown->from_user = true;
  shown->text = message;
  turns_.push_back(shown->Clone());
  Emit(*shown);

  busy_ = true;
  Step();
}

void AskSession::Step() {
  // Trimming the oldest two would cut between an assistant turn holding
  // tool_calls and the user turn holding their results, and a tool_result with
  // no tool_use before it is not a slightly odd transcript - both providers
  // reject the request outright. So drop from the front until what is left
  // starts on a plain user message.
  if (history_.size() > kMaxTurns) {
    size_t drop = 2;
    while (drop < history_.size() &&
           !(history_[drop].role == Message::Role::kUser &&
             history_[drop].tool_results.empty())) {
      drop++;
    }
    if (drop < history_.size())
      history_.erase(history_.begin(), history_.begin() + drop);
  }

  // The rendered thread is trimmed with it. It is cloned to the panel on every
  // restore, and a conversation nobody ended would grow without limit.
  while (turns_.size() > kMaxTurns) {
    turns_.erase(turns_.begin());
  }

  // Rebuilt whenever the model changes provider, not cached from the first
  // turn. The comment here used to say the model string decides which provider
  // is used, while `if (!provider_)` meant it decided once and never again -
  // so switching the intelligence selector to the other provider mid-thread
  // would have sent its model name to the wrong API. Every preset in the panel
  // is a Claude model today, so it was unreachable; a comment describing
  // behaviour the code does not have is how it stops being unreachable later.
  const bool anthropic = model_.starts_with("claude");
  if (!provider_ || anthropic != provider_is_anthropic_) {
    provider_ = anthropic
                    ? std::unique_ptr<LLMProvider>(
                          std::make_unique<AnthropicProvider>(profile_))
                    : std::unique_ptr<LLMProvider>(
                          std::make_unique<OpenAIProvider>(profile_));
    provider_is_anthropic_ = anthropic;
  }

  CompletionRequest request;
  request.model = model_;
  request.max_output_tokens = 4096;
  request.messages = CloneMessages(history_);
  request.tools = Tools();
  request.system_prompt = SystemPrompt();

  turn_started_at_ = base::TimeTicks::Now();
  provider_->Complete(std::move(request),
                      base::BindOnce(&AskSession::OnCompletion,
                                     weak_factory_.GetWeakPtr()));
}

void AskSession::OnCompletion(CompletionResponse response) {
  auto turn = mojom::AskTurn::New();
  turn->from_user = false;
  turn->thinking_ms = static_cast<uint32_t>(
      (base::TimeTicks::Now() - turn_started_at_).InMilliseconds());

  if (!response.error.empty()) {
    turn->text = response.error;
    turns_.push_back(turn->Clone());
    busy_ = false;
    Emit(*turn);
    return;
  }

  turn->text = response.text;

  Message assistant;
  assistant.role = Message::Role::kAssistant;
  assistant.text = response.text;
  assistant.tool_calls = CloneToolCalls(response.tool_calls);
  history_.push_back(std::move(assistant));

  if (response.tool_calls.empty()) {
    turns_.push_back(turn->Clone());
    busy_ = false;
    Emit(*turn);
    return;
  }

  pending_turn_ = std::move(turn);
  pending_calls_ = CloneToolCalls(response.tool_calls);
  pending_results_ = Message();
  pending_results_.role = Message::Role::kUser;
  next_call_ = 0;
  RunNextTool();
}

void AskSession::RunNextTool() {
  while (next_call_ < pending_calls_.size()) {
    const ToolCall& call = pending_calls_[next_call_];
    // ask_user stops the loop rather than returning: the answer comes from a
    // person, so there is nothing to hand back until they give it.
    if (call.name == "ask_user") {
      pending_question_call_id_ = call.id;
      auto request = mojom::QuestionRequest::New();
      request->run_id = "ask";
      if (const std::string* preamble = call.input.FindString("preamble"))
        request->preamble = *preamble;
      if (const base::ListValue* items = call.input.FindList("questions")) {
        for (const base::Value& item : *items) {
          const base::DictValue* dict = item.GetIfDict();
          if (!dict)
            continue;
          const std::string* text = dict->FindString("text");
          if (!text)
            continue;
          auto question = mojom::AgentQuestion::New();
          question->id = base::Uuid::GenerateRandomV4().AsLowercaseString();
          question->text = *text;
          if (const std::string* hint = dict->FindString("placeholder"))
            question->placeholder = *hint;
          if (const base::ListValue* choices = dict->FindList("choices")) {
            for (const base::Value& choice : *choices) {
              if (choice.is_string())
                question->choices.push_back(choice.GetString());
            }
          }
          request->questions.push_back(std::move(question));
        }
      }
      // Not busy: the session is waiting on a person, not working. Leaving
      // it busy disabled the composer, and since the question itself was not
      // part of the thread a reopened panel showed a dead input and nothing
      // to answer.
      busy_ = false;
      pending_question_ = request->Clone();
      turns_.push_back(pending_turn_->Clone());
      Emit(*pending_turn_);
      if (delegate_)
        delegate_->OnAskQuestions(*request);
      return;
    }

    if (call.name == "read_file" || call.name == "list_files") {
      // Off to a worker thread, so this returns and the chain resumes in
      // OnToolDone. Everything below it runs inline and falls through.
      RunFileTool(call, base::BindOnce(&AskSession::OnToolDone,
                                       weak_factory_.GetWeakPtr()));
      return;
    }

    // Synchronous: record and carry on round the loop. No recursion.
    RecordToolResult(RunTool(call));
  }

  if (!pending_turn_)
    return;
  history_.push_back(std::move(pending_results_));
  turns_.push_back(pending_turn_->Clone());
  Emit(*pending_turn_);
  pending_calls_.clear();
  pending_turn_.reset();
  next_call_ = 0;
  Step();
}

void AskSession::RecordToolResult(ToolResult result) {
  // Reset() can land between dispatching a worker and its reply. The weak
  // pointer covers destruction, not a thread the user started over on, so a
  // late result has to be dropped rather than appended to a turn that is no
  // longer being assembled.
  if (!pending_turn_ || next_call_ >= pending_calls_.size())
    return;
  result.tool_call_id = pending_calls_[next_call_].id;
  auto step = mojom::AskStep::New();
  step->label = result.content;
  step->succeeded = !result.is_error;
  pending_turn_->steps.push_back(std::move(step));
  pending_results_.tool_results.push_back(std::move(result));
  next_call_++;
}

void AskSession::OnToolDone(ToolResult result) {
  if (!pending_turn_)
    return;
  RecordToolResult(std::move(result));
  RunNextTool();
}

mojom::QuestionRequestPtr AskSession::PendingQuestion() const {
  return pending_question_ ? pending_question_->Clone() : nullptr;
}

void AskSession::Answer(std::vector<mojom::QuestionAnswerPtr> answers) {
  if (pending_question_call_id_.empty())
    return;
  pending_question_.reset();

  // text is `string?`, so it is std::optional here and not a string. Null is
  // documented as "skipped", which is a different thing from an empty answer -
  // the mojom says so explicitly, because "I am not telling you" and "there is
  // no value" lead the agent somewhere different.
  std::string joined;
  size_t answered = 0;
  for (const mojom::QuestionAnswerPtr& answer : answers) {
    if (!answer || !answer->text.has_value()) {
      base::StrAppend(&joined, {"(skipped)\n"});
      continue;
    }
    base::StrAppend(&joined, {*answer->text, "\n"});
    answered++;
  }

  ToolResult result;
  result.tool_call_id = pending_question_call_id_;
  result.content = answered > 0 ? joined : "(the user skipped these)";
  pending_question_call_id_.clear();

  Message message;
  message.role = Message::Role::kUser;
  message.tool_results.push_back(std::move(result));
  history_.push_back(std::move(message));

  busy_ = true;
  Step();
}

ToolResult AskSession::RunTool(const ToolCall& call) {
  ToolResult result;
  if (call.name == "save_workflow") {
    const std::string* command = call.input.FindString("command");
    const std::string* name = call.input.FindString("name");
    const std::string* prompt = call.input.FindString("prompt");
    if (!command || !name || !prompt || !scheduler_) {
      result.is_error = true;
      result.content = "save_workflow needs command, name and prompt.";
      return result;
    }
    Workflow workflow;
    workflow.command = *command;
    workflow.name = *name;
    if (const std::string* about = call.input.FindString("description"))
      workflow.description = *about;
    if (const std::string* cron = call.input.FindString("cron"))
      workflow.cron = *cron;
    if (const std::string* shown = call.input.FindString("schedule_display"))
      workflow.schedule_display = *shown;

    auto spec = mojom::TaskSpec::New();
    spec->prompt = *prompt;
    spec->write_scope = mojom::WriteScope::kDraft;
    spec->credit_budget = 100000;
    auto model = mojom::ModelConfig::New();
    model->provider = mojom::Provider::kAnthropic;
    model->model = "claude-sonnet-5";
    model->max_output_tokens = 8192;
    model->allow_failover = true;
    spec->model = std::move(model);
    workflow.spec = std::move(spec);

    const std::string id = scheduler_->Add(std::move(workflow));
    if (id.empty()) {
      result.is_error = true;
      result.content = base::StrCat({"Could not save /", *command, "."});
      return result;
    }
    result.content = base::StrCat({"Saved /", *command});
    return result;
  }

  if (call.name == "save_template") {
    const std::string* title = call.input.FindString("title");
    const std::string* prompt = call.input.FindString("prompt");
    if (!title || !prompt || !profile_) {
      result.is_error = true;
      result.content = "save_template needs a title and a prompt.";
      return result;
    }
    // Id derived from the title, so saving the same template twice edits it
    // rather than leaving two near-identical cards on the screen.
    std::string id;
    for (char c : *title) {
      if (base::IsAsciiAlphaNumeric(c))
        id += base::ToLowerASCII(c);
      else if (!id.empty() && id.back() != '-')
        id += '-';
    }
    while (!id.empty() && id.back() == '-')
      id.pop_back();
    if (id.empty()) {
      result.is_error = true;
      result.content = "That title has no characters an id can use.";
      return result;
    }

    base::DictValue entry;
    entry.Set("title", *title);
    if (const std::string* outcome = call.input.FindString("outcome"))
      entry.Set("outcome", *outcome);
    const std::string* category = call.input.FindString("category");
    entry.Set("category", category ? *category : "Ops");
    entry.Set("prompt", *prompt);
    entry.Set("write_scope",
              static_cast<int>(mojom::WriteScope::kReadOnly));

    ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kUserTemplates);
    update->Set(id, std::move(entry));
    result.content = base::StrCat({"Saved the template \"", *title, "\""});
    return result;
  }

  result.is_error = true;
  result.content = base::StrCat({"No such tool: ", call.name});
  return result;
}

void AskSession::RunFileTool(const ToolCall& call,
                             base::OnceCallback<void(ToolResult)> done) {
  const std::string* path = call.input.FindString("path");
  if (!path) {
    ToolResult result;
    result.is_error = true;
    result.content = base::StrCat({call.name, " needs a path."});
    std::move(done).Run(std::move(result));
    return;
  }

  auto finish = [](base::OnceCallback<void(ToolResult)> cb,
                   std::pair<std::string, bool> outcome) {
    ToolResult result;
    result.content = outcome.first;
    result.is_error = outcome.second;
    std::move(cb).Run(std::move(result));
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      call.name == "read_file"
          ? base::BindOnce(&ReadOnWorker, *path)
          : base::BindOnce(&ListOnWorker, *path),
      base::BindOnce(finish, std::move(done)));
}

std::vector<ToolDefinition> AskSession::Tools() const {
  std::vector<ToolDefinition> tools;

  {
    ToolDefinition save;
    save.name = "save_workflow";
    save.description =
        "Save a scheduled workflow for the user. Use this once you know what "
        "the task should do and when it should run - do not describe how to "
        "save one, save it. The user sees it on the Workflows screen and can "
        "edit or delete it there.";
    base::DictValue props;
    props.Set("command", StringProp(
        "Short slash command it runs by, lowercase and hyphenated, no slash."));
    props.Set("name", StringProp("Human name, a few words."));
    props.Set("description", StringProp("One line about what it produces."));
    props.Set("prompt", StringProp(
        "The full instruction the agent runs each time it fires. Write it as "
        "if handing the task to someone who has not read this conversation."));
    props.Set("cron", StringProp(
        "5-field cron in local time, e.g. '0 9,18 * * *' for 9am and 6pm "
        "daily. Omit for a workflow that only runs on demand."));
    props.Set("schedule_display", StringProp(
        "The schedule in words, e.g. 'Twice a day at 9am and 6pm'."));
    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(props));
    base::ListValue required;
    required.Append("command");
    required.Append("name");
    required.Append("prompt");
    schema.Set("required", std::move(required));
    save.input_schema = std::move(schema);
    tools.push_back(std::move(save));
  }

  {
    ToolDefinition save;
    save.name = "save_template";
    save.description =
        "Save a reusable task the user can start from later, shown on the "
        "Templates screen beside the ones Flux ships. Use this for something "
        "they will want to run again but not on a schedule - a workflow is "
        "the one that runs itself.";
    base::DictValue props;
    props.Set("title", StringProp("A few words, how they will recognise it."));
    props.Set("outcome", StringProp("One line about what it produces."));
    props.Set("category", StringProp(
        "One of Sales, Marketing, Ops, Engineering, Docs, Personal."));
    props.Set("prompt", StringProp(
        "The instruction the agent runs. Put anything the user must fill in "
        "each time in [square brackets] - the composer shows those as blanks "
        "and the agent asks about them rather than guessing."));
    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(props));
    base::ListValue required;
    required.Append("title");
    required.Append("prompt");
    schema.Set("required", std::move(required));
    save.input_schema = std::move(schema);
    tools.push_back(std::move(save));
  }

  {
    ToolDefinition list;
    list.name = "list_files";
    list.description =
        "List what is in one of the user's folders. You may only read inside "
        "Documents, Downloads and Desktop - anything else is refused. Use "
        "this to see what the user actually works with before proposing a "
        "template or a workflow, rather than guessing.";
    base::DictValue props;
    props.Set("path", StringProp("Absolute path to a folder."));
    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(props));
    base::ListValue required;
    required.Append("path");
    schema.Set("required", std::move(required));
    list.input_schema = std::move(schema);
    tools.push_back(std::move(list));
  }

  {
    ToolDefinition read;
    read.name = "read_file";
    read.description =
        "Read a text file of the user's. Same three folders as list_files, "
        "and read-only - nothing here can change or delete anything.";
    base::DictValue props;
    props.Set("path", StringProp("Absolute path to a file."));
    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(props));
    base::ListValue required;
    required.Append("path");
    schema.Set("required", std::move(required));
    read.input_schema = std::move(schema);
    tools.push_back(std::move(read));
  }

  {
    ToolDefinition ask;
    ask.name = "ask_user";
    ask.description =
        "Ask the user something you cannot work out yourself. Ask everything "
        "you need in one call rather than one question per turn. Offer "
        "choices whenever the sensible answers are a short list - the user "
        "taps one instead of typing.";
    base::DictValue question;
    question.Set("type", "object");
    base::DictValue qprops;
    qprops.Set("text", StringProp("The question."));
    qprops.Set("placeholder", StringProp("Hint shown in the empty field."));
    base::DictValue choices;
    choices.Set("type", "array");
    base::DictValue choice_items;
    choice_items.Set("type", "string");
    choices.Set("items", std::move(choice_items));
    choices.Set("description",
                "Pickable answers. Omit for a free-text question.");
    qprops.Set("choices", std::move(choices));
    question.Set("properties", std::move(qprops));

    base::DictValue list;
    list.Set("type", "array");
    list.Set("items", std::move(question));

    base::DictValue props;
    props.Set("preamble", StringProp("What you have worked out so far."));
    props.Set("questions", std::move(list));
    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(props));
    base::ListValue required;
    required.Append("questions");
    schema.Set("required", std::move(required));
    ask.input_schema = std::move(schema);
    tools.push_back(std::move(ask));
  }

  return tools;
}

std::string AskSession::SystemPrompt() const {
  std::string prompt =
      "You are Flux's built-in assistant, answering in a panel beside the "
      "user's browser.\n"
      "\n"
      "Flux is a web browser with an agent in it. The agent runs tasks in a "
      "real tab, signed in as the user already is, which is why it can do "
      "things a hosted tool cannot. What the user can do with it:\n"
      "- New task: describe something in plain words and the agent does it, "
      "browsing and using connectors as needed.\n"
      "- Templates: 250 ready-made tasks to start from.\n"
      "- Workflows: a saved task that re-runs on a schedule, reachable as a "
      "slash command.\n"
      "- Connectors: a direct API line into a service - faster and more "
      "reliable than clicking the site, but never required, because the "
      "browser is always the fallback.\n"
      "- Customize: standing instructions applied to every task, plus the "
      "skills the user has adopted.\n"
      "- Approvals: a task declared read-only stops and waits before doing "
      "anything that writes or sends.\n"
      "\n"
      "You can look at what the user actually has, in Documents, Downloads "
      "and Desktop only, with list_files and read_file. Nothing else on the "
      "machine is reachable and nothing you do can change or delete a file. "
      "Use it to ground a suggestion in their real work - a folder of "
      "invoices is a better basis for a template than a guess - and say what "
      "you looked at rather than presenting the conclusion on its own.\n"
      "\n"
      "Answer briefly and then act. If the user describes something they want "
      "to happen on a schedule, save it with save_workflow rather than "
      "explaining how they could save it themselves. Ask with ask_user when "
      "you are missing something only they know - a recipient, a link, which "
      "account - and offer choices when the answers are a short list. Never "
      "invent one of those values.\n"
      "\n"
      "Your replies are rendered as markdown.";

  // The user's own standing instructions apply here too. They wrote them for
  // the product, not for one screen of it.
  if (profile_) {
    const std::string& instructions =
        profile_->GetPrefs()->GetString(prefs::kInstructions);
    if (!instructions.empty()) {
      base::StrAppend(&prompt, {"\n\nStanding instructions from the user:\n",
                                instructions});
    }
  }
  return prompt;
}

void AskSession::Emit(const mojom::AskTurn& turn) {
  if (delegate_)
    delegate_->OnAskTurn(turn, busy_);
}

}  // namespace flux
