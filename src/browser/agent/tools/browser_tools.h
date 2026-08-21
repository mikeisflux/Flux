// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOLS_BROWSER_TOOLS_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOLS_BROWSER_TOOLS_H_

namespace flux {

class ToolRegistry;

// Registers the built-in browser tools:
//
//   navigate      kReadOnly   go to a URL
//   read_page     kReadOnly   accessibility-tree snapshot of the current page
//   find          kReadOnly   locate elements by accessible name/role
//   click         kReadOnly*  activate an element by node id
//   type          kReadOnly*  enter text into a field
//   select        kReadOnly   choose a dropdown option
//   scroll        kReadOnly   bring offscreen content into view
//   wait_for      kReadOnly   block until a condition holds
//   extract       kReadOnly   pull structured rows out of the page
//   screenshot    kReadOnly   capture pixels (for reporting, not for reasoning)
//   submit        kSend       submit a form - the scope boundary
//
// * click and type are read-only in themselves; what makes an interaction
//   consequential is submission. The runner additionally inspects click
//   targets for submit-like semantics (type=submit, role=button inside a form)
//   and escalates those to kSend, so "click the Send button" cannot slip
//   through as a read-only action.
void RegisterBrowserTools(ToolRegistry* registry);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOLS_BROWSER_TOOLS_H_
