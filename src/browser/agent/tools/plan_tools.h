// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_

namespace flux {

class FluxAgentService;
class ToolRegistry;

// Registers the tools that let a run say what it is doing, rather than leaving
// the console to infer it from a count of tool calls:
//
//   set_plan       kReadOnly   the steps this task will take, in order
//   complete_step  kReadOnly   mark one done, and say what is next
//   save_artifact  kReadOnly   record a file the run produced
//
// All three are kReadOnly on purpose. They change nothing outside Flux - a
// plan is a statement of intent and an artifact is a pointer to a file the
// run already made by other means, each of which was gated on its own scope
// when it happened. Requiring a higher scope here would mean a read-only task
// could not tell the user what it was doing, which is exactly backwards.
void RegisterPlanTools(ToolRegistry* registry, FluxAgentService* service);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_
