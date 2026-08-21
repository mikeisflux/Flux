// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/scheduler/workflow_scheduler.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "chrome/browser/flux/flux_agent_service.h"

namespace flux {
namespace {

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
    if (!base::Contains(minutes, exploded.minute))
      continue;
    if (!base::Contains(hours, exploded.hour))
      continue;
    if (!base::Contains(months, exploded.month))
      continue;

    // cron's historical quirk: when both day-of-month and day-of-week are
    // restricted, a match on EITHER fires. Getting this wrong makes
    // "1st of the month" and "every Monday" silently wrong together.
    const bool dom_match = base::Contains(doms, exploded.day_of_month);
    const bool dow_match = base::Contains(dows, exploded.day_of_week);
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
  return id;
}

void WorkflowScheduler::Remove(const std::string& workflow_id) {
  workflows_.erase(workflow_id);
  ScheduleNext();
}

void WorkflowScheduler::SetEnabled(const std::string& workflow_id,
                                   bool enabled) {
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end())
    return;
  it->second.enabled = enabled;
  ScheduleNext();
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
    workflow.last_run = now;

    if (service_ && workflow.spec) {
      std::string error;
      service_->StartRun(workflow.spec->Clone(), &error);
    }

    std::optional<base::Time> next = NextFireTime(workflow.cron, now);
    workflow.next_run = next.value_or(base::Time());
  }
  ScheduleNext();
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
