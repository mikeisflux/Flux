// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/tools/plan_tools.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "url/gurl.h"

namespace flux {
namespace {

// Same shape the other tool files use: a plain struct with two factories,
// rather than each call site building one by hand.
ToolResult Ok(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  return r;
}

ToolResult Err(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  r.is_error = true;
  return r;
}

// A remembered fact lands in prefs as plaintext. The model cannot reliably
// tell a credential from a note, so this refuses the prefixes that are only
// ever credentials.
//
// Prefixes only, deliberately. The first version also rejected any unbroken
// 32-character run of key-shaped characters, which reads like a sensible
// entropy check and rejects "their tracker is at
// docs.google.com/spreadsheets/d/1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms"
// - a Google Sheet id is 44 characters of exactly that alphabet, and a link to
// the user's own sheet is the single most useful thing this tool can hold. It
// also matched "password" and "secret" as substrings, which rejects "they use
// 1Password". A guard that blocks the feature it is protecting is worse than
// no guard.
bool LooksLikeASecret(const std::string& text) {
  static constexpr const char* kMarkers[] = {
      "sk-", "xoxb-", "xoxp-", "xapp-", "ghp_",     "gho_",
      "github_pat_", "AKIA", "ASIA", "Bearer ", "api_key=", "apikey=",
      "-----BEGIN",
  };
  for (const char* marker : kMarkers) {
    if (text.find(marker) != std::string::npos)
      return true;
  }
  return false;
}

base::DictValue StringProperty(const std::string& description) {
  base::DictValue property;
  property.Set("type", "string");
  property.Set("description", description);
  return property;
}

// Every plan tool needs the service to write through, and none of them touch
// the page. Sharing the base keeps the three of them to their differences.
class PlanToolBase : public Tool {
 public:
  explicit PlanToolBase(FluxAgentService* service) : service_(service) {}

  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }

 protected:
  raw_ptr<FluxAgentService> service_;
};

class SetPlanTool : public PlanToolBase {
 public:
  using PlanToolBase::PlanToolBase;

  std::string name() const override { return "set_plan"; }

  std::string description() const override {
    return "State the steps this task will take, in order, before starting "
           "work. Three to six steps, each one a thing the user would "
           "recognise as done or not done - not internal bookkeeping. Call "
           "this once, first. The steps are shown to the user for the whole "
           "run, so write them for them.";
  }

  base::DictValue InputSchema() const override {
    base::DictValue steps;
    steps.Set("type", "array");
    steps.Set("description", "The steps, in the order they will happen.");
    base::DictValue items;
    items.Set("type", "string");
    steps.Set("items", std::move(items));

    base::DictValue properties;
    properties.Set("steps", std::move(steps));

    base::ListValue required;
    required.Append("steps");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const base::ListValue* steps = input.FindList("steps");
    return base::StrCat(
        {"Set a plan of ", base::NumberToString(steps ? steps->size() : 0u),
         " steps"});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const base::ListValue* raw = input.FindList("steps");
    if (!raw || raw->empty()) {
      std::move(callback).Run(
          Err("A plan needs at least one step."));
      return;
    }

    std::vector<mojom::TaskStepPtr> plan;
    for (const base::Value& entry : *raw) {
      if (!entry.is_string() || entry.GetString().empty())
        continue;
      auto step = mojom::TaskStep::New();
      step->text = entry.GetString();
      step->state = mojom::TaskStepState::kPending;
      plan.push_back(std::move(step));
    }
    if (plan.empty()) {
      std::move(callback).Run(
          Err("Every step was empty."));
      return;
    }

    // The first step starts active. A plan where nothing is happening reads as
    // a plan that has not been started.
    plan.front()->state = mojom::TaskStepState::kActive;

    const size_t count = plan.size();
    if (service_)
      service_->SetPlan(context.run_id, std::move(plan));
    std::move(callback).Run(Ok(base::StrCat(
        {"Plan set with ", base::NumberToString(count),
         " steps. Call complete_step as you finish each one."})));
  }
};

class CompleteStepTool : public PlanToolBase {
 public:
  using PlanToolBase::PlanToolBase;

  std::string name() const override { return "complete_step"; }

  std::string description() const override {
    return "Mark a plan step finished and start the next one. Call this as "
           "soon as a step is actually done, not in a batch at the end - the "
           "point of the plan is that the user can see where the task is "
           "while it is still running.";
  }

  base::DictValue InputSchema() const override {
    base::DictValue index;
    index.Set("type", "integer");
    index.Set("description",
              "Zero-based index of the step being completed.");

    base::DictValue skipped;
    skipped.Set("type", "boolean");
    skipped.Set("description",
                "True if the step turned out not to be needed. Skipped is "
                "honest; silently marking it done is not.");

    base::DictValue properties;
    properties.Set("index", std::move(index));
    properties.Set("skipped", std::move(skipped));

    base::ListValue required;
    required.Append("index");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    return base::StrCat({"Mark plan step ",
                         base::NumberToString(input.FindInt("index").value_or(0)),
                         " complete"});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const std::optional<int> index = input.FindInt("index");
    if (!index || *index < 0) {
      std::move(callback).Run(
          Err("complete_step needs a zero-based index."));
      return;
    }
    const bool skipped = input.FindBool("skipped").value_or(false);

    if (service_) {
      service_->AdvancePlan(context.run_id, static_cast<uint32_t>(*index),
                            skipped ? mojom::TaskStepState::kSkipped
                                    : mojom::TaskStepState::kDone);
      // The next one becomes active, so the panel always shows where the run
      // is rather than a gap between one done and the next started.
      service_->AdvancePlan(context.run_id,
                            static_cast<uint32_t>(*index) + 1,
                            mojom::TaskStepState::kActive);
    }
    std::move(callback).Run(Ok("Recorded."));
  }
};

class SaveArtifactTool : public PlanToolBase {
 public:
  using PlanToolBase::PlanToolBase;

  std::string name() const override { return "save_artifact"; }

  std::string description() const override {
    return "Record a file this task produced so the user can open it from the "
           "run. Call this for the actual deliverable - the sheet, the "
           "report, the document - not for scratch files. Describing a file "
           "in your summary without saving it here leaves the user with "
           "nothing to click.";
  }

  base::DictValue InputSchema() const override {
    base::DictValue files;
    files.Set("type", "array");
    files.Set("description",
              "Secondary files that belong with it, if any.");
    base::DictValue item;
    item.Set("type", "object");
    base::DictValue item_properties;
    item_properties.Set("name", StringProperty("What to call it."));
    item_properties.Set("url", StringProperty("Where it is."));
    item.Set("properties", std::move(item_properties));
    files.Set("items", std::move(item));

    base::DictValue properties;
    properties.Set("title", StringProperty("What the user asked for, named."));
    properties.Set(
        "kind", StringProperty("Webpage, Spreadsheet, Document, and so on."));
    properties.Set("url", StringProperty("Where the main file is."));
    properties.Set("files", std::move(files));

    base::ListValue required;
    required.Append("title");
    required.Append("url");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* title = input.FindString("title");
    return base::StrCat({"Record the file \"", title ? *title : "untitled",
                         "\" against this run"});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const std::string* title = input.FindString("title");
    const std::string* url = input.FindString("url");
    if (!title || !url) {
      std::move(callback).Run(
          Err("save_artifact needs a title and a url."));
      return;
    }

    GURL parsed(*url);
    if (!parsed.is_valid()) {
      std::move(callback).Run(Err(
          base::StrCat({"\"", *url, "\" is not a URL that can be opened."})));
      return;
    }

    auto artifact = mojom::RunArtifact::New();
    artifact->title = *title;
    if (const std::string* kind = input.FindString("kind"))
      artifact->kind = *kind;
    artifact->url = parsed;

    if (const base::ListValue* files = input.FindList("files")) {
      for (const base::Value& entry : *files) {
        if (!entry.is_dict())
          continue;
        const std::string* name = entry.GetDict().FindString("name");
        const std::string* href = entry.GetDict().FindString("url");
        if (!name || !href)
          continue;
        GURL file_url(*href);
        if (!file_url.is_valid())
          continue;
        auto file = mojom::ArtifactFile::New();
        file->name = *name;
        file->url = file_url;
        artifact->files.push_back(std::move(file));
      }
    }

    if (service_)
      service_->AddArtifact(context.run_id, std::move(artifact));
    std::move(callback).Run(
        Ok("Saved. The user can open it from the run."));
  }
};

// Fanning work out to children only pays when the pieces are genuinely
// independent. Anything that has to happen in order belongs in the plan, not
// here: four children each opening the same site and racing each other to the
// same form is slower than doing it once and considerably worse.
class SpawnSubagentsTool : public PlanToolBase {
 public:
  using PlanToolBase::PlanToolBase;

  std::string name() const override { return "spawn_subagents"; }

  std::string description() const override {
    return "Split independent work across parallel child runs and wait for "
           "all of them. Use it when the task is the same shape repeated over "
           "different inputs - ten companies to research, six pages to "
           "summarise - and the pieces do not depend on each other. Each "
           "child gets its own browser and its own share of the budget, and "
           "you get their results back together. Do not use it for steps that "
           "have to happen in order, or for work that touches the same page: "
           "that is what the plan is for.";
  }

  base::DictValue InputSchema() const override {
    base::DictValue item;
    item.Set("type", "object");
    base::DictValue item_properties;
    item_properties.Set(
        "label", StringProperty(
                     "A few words naming this piece, shown to the user as the "
                     "child's row. \"Research Acme Corp\", not \"task 1\"."));
    item_properties.Set(
        "prompt",
        StringProperty("The complete instruction for this child. It cannot "
                       "see your conversation, so restate everything it "
                       "needs, including what to report back."));
    item.Set("properties", std::move(item_properties));
    base::ListValue item_required;
    item_required.Append("label");
    item_required.Append("prompt");
    item.Set("required", std::move(item_required));

    base::DictValue tasks;
    tasks.Set("type", "array");
    tasks.Set("description",
              "The independent pieces of work, at most eight.");
    tasks.Set("items", std::move(item));

    base::DictValue properties;
    properties.Set("tasks", std::move(tasks));

    base::ListValue required;
    required.Append("tasks");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const base::ListValue* tasks = input.FindList("tasks");
    return base::StrCat({"Run ",
                         base::NumberToString(tasks ? tasks->size() : 0u),
                         " subagents in parallel"});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const base::ListValue* raw = input.FindList("tasks");
    if (!raw || raw->empty()) {
      std::move(callback).Run(Err("spawn_subagents needs at least one task."));
      return;
    }

    std::vector<std::pair<std::string, std::string>> work;
    std::vector<std::string> labels;
    for (const base::Value& entry : *raw) {
      if (!entry.is_dict())
        continue;
      const std::string* label = entry.GetDict().FindString("label");
      const std::string* prompt = entry.GetDict().FindString("prompt");
      if (!label || !prompt || prompt->empty())
        continue;
      labels.push_back(*label);
      work.emplace_back(*label, *prompt);
    }
    if (work.empty()) {
      std::move(callback).Run(
          Err("Every task was missing a label or a prompt."));
      return;
    }
    if (!service_) {
      std::move(callback).Run(Err("Subagents are not available."));
      return;
    }

    // Nothing is returned until the last child lands. That is deliberate: the
    // point of the call is to get all of the answers, and a partial result
    // handed back early would just be resumed into a second wait.
    service_->StartSubagents(
        context.run_id, std::move(work),
        base::BindOnce(&SpawnSubagentsTool::OnBatchFinished,
                       weak_factory_.GetWeakPtr(), std::move(labels),
                       std::move(callback)));
  }

 private:
  void OnBatchFinished(std::vector<std::string> labels,
                       ResultCallback callback,
                       std::vector<std::string> summaries,
                       const std::string& error) {
    // The whole call can be refused - one level of nesting only, a batch
    // already outstanding - in which case no child ran and there is nothing to
    // pair against the labels.
    if (!error.empty()) {
      std::move(callback).Run(Err(error));
      return;
    }
    if (summaries.size() != labels.size()) {
      std::move(callback).Run(Err("Subagents could not be started."));
      return;
    }

    std::string report;
    for (size_t i = 0; i < summaries.size(); ++i) {
      base::StrAppend(&report, {"## ", labels[i], "\n",
                                summaries[i].empty() ? "(no result reported)"
                                                     : summaries[i],
                                "\n\n"});
    }
    std::move(callback).Run(Ok(std::move(report)));
  }

  base::WeakPtrFactory<SpawnSubagentsTool> weak_factory_{this};
};

// The only tool here that writes something the user keeps. It is still
// kReadOnly: a note in Flux's own prefs is not an effect on the world, and
// gating it would mean a read-only task could not remember what it found out.
class RememberTool : public PlanToolBase {
 public:
  using PlanToolBase::PlanToolBase;

  std::string name() const override { return "remember"; }

  std::string description() const override {
    return "Record something durable you learned about how the user works, so "
           "a later task does not have to work it out again. Good: which CRM "
           "they use, the sheet a tracker lives in, that their team channel is "
           "#growth, the name their invoices go out under. Not for anything "
           "specific to this one task, anything that will be stale next week, "
           "or anything they told you in the prompt - they already know that. "
           "The user sees every fact and can delete any of them.";
  }

  base::DictValue InputSchema() const override {
    base::DictValue properties;
    properties.Set(
        "fact", StringProperty(
                    "One sentence, in plain language, written so it still "
                    "makes sense read on its own in three months."));

    base::ListValue required;
    required.Append("fact");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* fact = input.FindString("fact");
    return base::StrCat({"Remember: ", fact ? *fact : ""});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const std::string* fact = input.FindString("fact");
    if (!fact || fact->empty()) {
      std::move(callback).Run(Err("remember needs a fact."));
      return;
    }
    // Nothing sensitive. A credential in the prefs file is a credential in
    // plaintext, and the model has no way to know what it is holding.
    if (LooksLikeASecret(*fact)) {
      std::move(callback).Run(
          Err("That looks like a credential. Those belong in Connectors, "
              "where they are stored encrypted - not in a remembered note."));
      return;
    }
    if (service_)
      service_->RememberFact(context.run_id, *fact);
    std::move(callback).Run(Ok("Noted. The user can see and delete this."));
  }
};

}  // namespace

void RegisterPlanTools(ToolRegistry* registry, FluxAgentService* service) {
  registry->Register(std::make_unique<SetPlanTool>(service));
  registry->Register(std::make_unique<CompleteStepTool>(service));
  registry->Register(std::make_unique<SaveArtifactTool>(service));
  registry->Register(std::make_unique<SpawnSubagentsTool>(service));
  registry->Register(std::make_unique<RememberTool>(service));
}

}  // namespace flux
