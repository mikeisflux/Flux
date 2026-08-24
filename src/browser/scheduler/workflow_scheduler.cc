// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/scheduler/workflow_scheduler.h"

#include <algorithm>
#include <utility>

// Membership tests below use std::find rather than the base:: helper, whose
// header does not exist at this revision.

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/flux/flux_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flux {
namespace {

// What a workflow restored without a usable budget gets. Matches the composer
// and the workflow dialog's "Medium".
constexpr uint64_t kDefaultCreditBudget = 100000;

// And what one restored without a model gets, for the same reason. These match
// the dialog's "Medium" so a repaired record behaves like a freshly saved one.
constexpr char kDefaultModel[] = "claude-sonnet-5";
constexpr int kDefaultMaxOutputTokens = 8192;

// Expands one cron field into the set of values it matches.
// Supports `*`, `*/n`, `a-b`, and comma lists - the subset the catalog's
// schedules actually use.
bool ExpandField(const std::string& field,
                 int min,
                 int max,
                 std::vector<int>* out) {
  for (const std::string& part : base::SplitString(
           field, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    int step = 1;
    std::string range = part;

    const size_t slash = part.find('/');
    if (slash != std::string::npos) {
      range = part.substr(0, slash);
      if (!base::StringToInt(part.substr(slash + 1), &step) || step <= 0)
        return false;
    }

    int lo = min;
    int hi = max;
    if (range != "*") {
      const size_t dash = range.find('-');
      if (dash == std::string::npos) {
        if (!base::StringToInt(range, &lo))
          return false;
        hi = lo;
      } else {
        if (!base::StringToInt(range.substr(0, dash), &lo) ||
            !base::StringToInt(range.substr(dash + 1), &hi)) {
          return false;
        }
      }
    }
    if (lo < min || hi > max || lo > hi)
      return false;
    for (int v = lo; v <= hi; v += step)
      out->push_back(v);
  }
  std::sort(out->begin(), out->end());
  out->erase(std::unique(out->begin(), out->end()), out->end());
  return !out->empty();
}

}  // namespace

WorkflowScheduler::WorkflowScheduler(FluxAgentService* service)
    : service_(service) {}

WorkflowScheduler::~WorkflowScheduler() = default;

// static
std::optional<base::Time> WorkflowScheduler::NextFireTime(
    const std::string& cron,
    base::Time after) {
  const std::vector<std::string> fields = base::SplitString(
      cron, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (fields.size() != 5)
    return std::nullopt;

  std::vector<int> minutes, hours, doms, months, dows;
  if (!ExpandField(fields[0], 0, 59, &minutes) ||
      !ExpandField(fields[1], 0, 23, &hours) ||
      !ExpandField(fields[2], 1, 31, &doms) ||
      !ExpandField(fields[3], 1, 12, &months) ||
      !ExpandField(fields[4], 0, 6, &dows)) {
    return std::nullopt;
  }

  const bool dom_restricted = fields[2] != "*";
  const bool dow_restricted = fields[4] != "*";

  // Walk forward a minute at a time from the next whole minute. Crude, but a
  // year of minutes is half a million iterations of trivial work and it avoids
  // an entire class of calendar bugs.
  base::Time candidate = after + base::Minutes(1);
  base::Time::Exploded exploded;
  for (int i = 0; i < 366 * 24 * 60; ++i, candidate += base::Minutes(1)) {
    candidate.LocalExplode(&exploded);
    if (std::find(minutes.begin(), minutes.end(), exploded.minute) == minutes.end())
      continue;
    if (std::find(hours.begin(), hours.end(), exploded.hour) == hours.end())
      continue;
    if (std::find(months.begin(), months.end(), exploded.month) == months.end())
      continue;

    // cron's historical quirk: when both day-of-month and day-of-week are
    // restricted, a match on EITHER fires. Getting this wrong makes
    // "1st of the month" and "every Monday" silently wrong together.
    const bool dom_match = std::find(doms.begin(), doms.end(), exploded.day_of_month) != doms.end();
    const bool dow_match = std::find(dows.begin(), dows.end(), exploded.day_of_week) != dows.end();
    if (dom_restricted && dow_restricted) {
      if (!dom_match && !dow_match)
        continue;
    } else if (dom_restricted && !dom_match) {
      continue;
    } else if (dow_restricted && !dow_match) {
      continue;
    }

    // Zero the seconds so repeated computation is stable.
    exploded.second = 0;
    exploded.millisecond = 0;
    base::Time fire;
    if (base::Time::FromLocalExploded(exploded, &fire))
      return fire;
  }
  return std::nullopt;
}

PrefService* WorkflowScheduler::Prefs() const {
  Profile* profile = service_ ? service_->profile() : nullptr;
  return profile ? profile->GetPrefs() : nullptr;
}

namespace {

// One workflow as a pref dictionary. TaskSpec is flattened rather than nested:
// the spec is five scalars and a nested dict buys nothing but another level to
// get wrong when reading it back.
base::DictValue ToDict(const Workflow& workflow) {
  base::DictValue out;
  out.Set("command", workflow.command);
  out.Set("name", workflow.name);
  out.Set("description", workflow.description);
  out.Set("cron", workflow.cron);
  out.Set("schedule_display", workflow.schedule_display);
  out.Set("enabled", workflow.enabled);
  out.Set("last_fire_missed", workflow.last_fire_missed);
  // Times as microseconds since the Windows epoch, which is what
  // base::Time::ToDeltaSinceWindowsEpoch gives on every platform - a double
  // would lose precision on a value this large.
  out.Set("last_run",
          base::NumberToString(
              workflow.last_run.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  if (workflow.spec) {
    out.Set("prompt", workflow.spec->prompt);
    if (workflow.spec->template_id)
      out.Set("template_id", *workflow.spec->template_id);
    out.Set("write_scope", static_cast<int>(workflow.spec->write_scope));
    out.Set("profile_id", workflow.spec->profile_id);
    out.Set("credit_budget",
            base::NumberToString(workflow.spec->credit_budget));
    // The model was the one field this pair never carried, and its absence is
    // not a degraded run - TaskSpec::model is dereferenced unguarded when a run
    // starts, so a workflow restored without one took the browser process down
    // with a CHECK the moment "Run now" was clicked.
    if (workflow.spec->model) {
      out.Set("provider", static_cast<int>(workflow.spec->model->provider));
      out.Set("model", workflow.spec->model->model);
      out.Set("max_output_tokens",
              static_cast<int>(workflow.spec->model->max_output_tokens));
      out.Set("allow_failover", workflow.spec->model->allow_failover);
    }
  }
  return out;
}

base::Time TimeFromDict(const base::DictValue& dict, std::string_view key) {
  const std::string* raw = dict.FindString(key);
  int64_t micros = 0;
  if (!raw || !base::StringToInt64(*raw, &micros))
    return base::Time();
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(micros));
}

}  // namespace

void WorkflowScheduler::SaveToPrefs() const {
  PrefService* prefs = Prefs();
  if (!prefs)
    return;
  base::DictValue all;
  for (const auto& [id, workflow] : workflows_)
    all.Set(id, ToDict(workflow));
  prefs->SetDict(prefs::kWorkflows, std::move(all));
}

void WorkflowScheduler::LoadFromPrefs() {
  PrefService* prefs = Prefs();
  if (!prefs)
    return;

  workflows_.clear();
  for (const auto entry : prefs->GetDict(prefs::kWorkflows)) {
    if (!entry.second.is_dict())
      continue;
    const base::DictValue& dict = entry.second.GetDict();

    Workflow workflow;
    workflow.id = entry.first;
    if (const std::string* v = dict.FindString("command"))
      workflow.command = *v;
    if (const std::string* v = dict.FindString("name"))
      workflow.name = *v;
    if (const std::string* v = dict.FindString("description"))
      workflow.description = *v;
    if (const std::string* v = dict.FindString("cron"))
      workflow.cron = *v;
    if (const std::string* v = dict.FindString("schedule_display"))
      workflow.schedule_display = *v;
    workflow.enabled = dict.FindBool("enabled").value_or(true);
    workflow.last_fire_missed =
        dict.FindBool("last_fire_missed").value_or(false);
    workflow.last_run = TimeFromDict(dict, "last_run");

    auto spec = mojom::TaskSpec::New();
    if (const std::string* v = dict.FindString("prompt"))
      spec->prompt = *v;
    if (const std::string* v = dict.FindString("template_id"))
      spec->template_id = *v;
    spec->write_scope = static_cast<mojom::WriteScope>(
        dict.FindInt("write_scope")
            .value_or(static_cast<int>(mojom::WriteScope::kReadOnly)));
    if (const std::string* v = dict.FindString("profile_id"))
      spec->profile_id = *v;
    // A zero budget is refused by StartRun, so a workflow restored with one
    // can never fire - and it would fail that way silently every morning,
    // for good. Workflows saved before the dialog supplied a budget are all
    // in that state, so repair on load rather than leaving them broken: the
    // user cannot tell from the row that the record is the problem.
    uint64_t budget = 0;
    if (const std::string* v = dict.FindString("credit_budget"))
      base::StringToUint64(*v, &budget);
    spec->credit_budget = budget > 0 ? budget : kDefaultCreditBudget;

    // Always constructed, never left null. Every workflow saved before the
    // serializer carried a model has none of these keys, so this is also the
    // repair path for those records - and the alternative to repairing them is
    // a browser that dies when the user clicks Run.
    auto model = mojom::ModelConfig::New();
    // Range-checked rather than cast straight through: the value comes off
    // disk, and static_cast to an enum with no matching value is undefined.
    const int provider = dict.FindInt("provider").value_or(
        static_cast<int>(mojom::Provider::kAnthropic));
    model->provider = provider == static_cast<int>(mojom::Provider::kOpenAI)
                          ? mojom::Provider::kOpenAI
                          : mojom::Provider::kAnthropic;
    const std::string* model_name = dict.FindString("model");
    model->model = (model_name && !model_name->empty()) ? *model_name
                                                        : kDefaultModel;
    model->max_output_tokens = static_cast<uint32_t>(std::max(
        1, dict.FindInt("max_output_tokens").value_or(
               kDefaultMaxOutputTokens)));
    model->allow_failover = dict.FindBool("allow_failover").value_or(true);
    spec->model = std::move(model);

    workflow.spec = std::move(spec);

    // Recomputed rather than restored: the saved next_run is in the past by
    // definition after a restart, and a stale one would fire immediately.
    if (std::optional<base::Time> next =
            NextFireTime(workflow.cron, base::Time::Now())) {
      workflow.next_run = *next;
    }

    workflows_[workflow.id] = std::move(workflow);
  }
  ScheduleNext();
}

const Workflow* WorkflowScheduler::Get(const std::string& workflow_id) const {
  auto it = workflows_.find(workflow_id);
  return it == workflows_.end() ? nullptr : &it->second;
}

std::string WorkflowScheduler::Add(Workflow workflow) {
  if (workflow.id.empty())
    workflow.id = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::optional<base::Time> next =
      NextFireTime(workflow.cron, base::Time::Now());
  if (next)
    workflow.next_run = *next;

  const std::string id = workflow.id;
  workflows_[id] = std::move(workflow);
  ScheduleNext();
  SaveToPrefs();
  return id;
}

void WorkflowScheduler::Remove(const std::string& workflow_id) {
  workflows_.erase(workflow_id);
  ScheduleNext();
  SaveToPrefs();
}

void WorkflowScheduler::SetEnabled(const std::string& workflow_id,
                                   bool enabled) {
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end())
    return;
  it->second.enabled = enabled;
  ScheduleNext();
  SaveToPrefs();
}

std::vector<const Workflow*> WorkflowScheduler::List() const {
  std::vector<const Workflow*> out;
  out.reserve(workflows_.size());
  for (const auto& [id, workflow] : workflows_)
    out.push_back(&workflow);
  return out;
}

void WorkflowScheduler::ScheduleNext() {
  base::Time soonest = base::Time::Max();
  for (const auto& [id, workflow] : workflows_) {
    if (workflow.enabled && !workflow.next_run.is_null())
      soonest = std::min(soonest, workflow.next_run);
  }
  if (soonest == base::Time::Max()) {
    timer_.Stop();
    return;
  }
  // WallClockTimer rather than a delay timer: it survives suspend and fires
  // against the actual clock, which is what a 7am schedule means.
  timer_.Start(FROM_HERE, soonest,
               base::BindOnce(&WorkflowScheduler::OnTimerFired,
                              weak_factory_.GetWeakPtr()));
}

void WorkflowScheduler::OnTimerFired() {
  const base::Time now = base::Time::Now();

  for (auto& [id, workflow] : workflows_) {
    if (!workflow.enabled || workflow.next_run.is_null() ||
        workflow.next_run > now) {
      continue;
    }

    // If the machine was asleep through one or more firings, run once and
    // record that the rest were missed. Firing the backlog would produce a
    // burst of duplicate work, which is worse than skipping it.
    workflow.last_fire_missed = (now - workflow.next_run) > base::Minutes(5);

    // last_run is set only if a run actually started. It used to be stamped
    // unconditionally, so a workflow that could not start - no API key, no
    // budget - showed "Last run: 2 minutes ago" in the table with no run
    // anywhere to show for it, which is the most misleading state the row has.
    if (service_ && workflow.spec) {
      std::string error;
      if (service_->StartRun(workflow.spec->Clone(), &error))
        workflow.last_run = now;
      else
        LOG(WARNING) << "Flux workflow " << id << " did not start: " << error;
    }

    std::optional<base::Time> next = NextFireTime(workflow.cron, now);
    workflow.next_run = next.value_or(base::Time());
  }
  ScheduleNext();
  // last_run and last_fire_missed changed above. Without this the table says
  // "Never run" forever, and a missed firing is forgotten by the next restart.
  SaveToPrefs();
}

// static
bool WorkflowScheduler::CanCompile(
    const std::vector<mojom::ActionRecordPtr>& actions,
    std::string* reason) {
  for (const auto& action : actions) {
    if (!action->succeeded) {
      *reason =
          "This run recovered from a failed step, so replaying it would repeat "
          "the recovery rather than the working path.";
      return false;
    }
    if (action->was_approved) {
      *reason =
          "This run required approval partway through. Compiling it would "
          "replay that action unattended, so it stays a manual task.";
      return false;
    }
  }
  if (actions.empty()) {
    *reason = "Nothing to replay.";
    return false;
  }
  return true;
}

std::optional<std::string> WorkflowScheduler::CompileWorkflowFromActions(
    const std::string& run_id,
    const std::vector<mojom::ActionRecordPtr>& actions,
    std::string* error) {
  if (!CanCompile(actions, error))
    return std::nullopt;

  Workflow workflow;
  workflow.name = "Replay of " + run_id;
  workflow.command = "replay-" + run_id.substr(0, 8);
  // Compiled workflows are created disabled and unscheduled. The user decides
  // when and whether a replay runs; silently scheduling a copy of something
  // they ran once would be a surprise.
  workflow.enabled = false;

  const std::string id = Add(std::move(workflow));
  return id;
}

}  // namespace flux
