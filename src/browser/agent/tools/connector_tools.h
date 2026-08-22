// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOLS_CONNECTOR_TOOLS_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOLS_CONNECTOR_TOOLS_H_

namespace flux {

class ToolRegistry;

// Registers the tools that let a run use a connected service:
//
//   list_connectors   kReadOnly   what is connected, and what it can do
//   connector_read    kReadOnly   run a readonly operation
//   connector_draft   kDraft      run a readonly or draft operation
//   connector_send    kSend       run a readonly, draft or send operation
//
// THREE TOOLS RATHER THAN ONE, and the reason is the whole point of the scope
// system. Tool::RequiredScope() is fixed per tool, but a connector's
// operations are not: basecamp.list_projects is readonly and
// basecamp.post_message sends. One `connector_call` tool would have to claim
// the highest scope any operation might need, and then either be withheld from
// every read-only task or be offered and refused - and ToolRegistry is built
// on the opposite principle, that a tool above the task's scope is never shown
// at all, so the model does not spend turns arguing for it.
//
// Splitting by tier keeps that true. A read-only task is handed
// connector_read and cannot see the others; each tool passes ITS OWN tier to
// the client as the granted scope, never the task's, so connector_read can
// only ever reach a readonly operation even inside a task allowed to send.
void RegisterConnectorTools(ToolRegistry* registry);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOLS_CONNECTOR_TOOLS_H_
