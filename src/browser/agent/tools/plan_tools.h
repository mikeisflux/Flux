// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_

namespace flux {

class FluxAgentService;
class ToolRegistry;

// Registers the tools that let a run say what it is doing, rather than leaving
// the console to infer it from a count of tool calls:
//
//   set_plan         kReadOnly   the steps this task will take, in order
//   complete_step    kReadOnly   mark one done, and say what is next
//   save_artifact    kReadOnly   record a file the run produced
//   spawn_subagents  kReadOnly   fan independent work out to child runs
//
// All four are kReadOnly on purpose. They change nothing outside Flux - a
// plan is a statement of intent and an artifact is a pointer to a file the
// run already made by other means, each of which was gated on its own scope
// when it happened. Requiring a higher scope here would mean a read-only task
// could not tell the user what it was doing, which is exactly backwards.
//
// spawn_subagents is the one that looks like it should be higher, and is not.
// It performs no effect itself: each child is started with the parent's own
// WriteScope and every tool the child calls is gated against that scope by the
// child's own runner, so the approvals happen where the effects do. Scoping
// the spawn instead would prompt the user for an approval that names no page
// and no payload - the least useful prompt in the product - and then still
// prompt again for the real write.
void RegisterPlanTools(ToolRegistry* registry, FluxAgentService* service);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOLS_PLAN_TOOLS_H_
