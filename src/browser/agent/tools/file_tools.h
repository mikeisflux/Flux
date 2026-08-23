// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOLS_FILE_TOOLS_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOLS_FILE_TOOLS_H_

namespace flux {

class FluxAgentService;
class ToolRegistry;

// write_file: the agent hands the user an actual file.
//
// Everything else the agent can produce lives somewhere else - a Google Sheet,
// a Doc, a page it navigated to - and save_artifact records a URL to it. A
// great many of the tasks in the catalogue end in something that has no URL: a
// CSV of scraped rows, a report, a JSON export, a list to paste elsewhere.
// Without this the agent's only option is to print a thousand rows into the
// chat and let the user select them by hand.
//
// kDraft, not kReadOnly. It writes a real file to the user's disk, which is a
// side effect they should have agreed to - but it transmits nothing, so it
// does not need kSend.
void RegisterFileTools(ToolRegistry* registry, FluxAgentService* service);

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOLS_FILE_TOOLS_H_
