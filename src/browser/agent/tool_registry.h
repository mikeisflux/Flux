// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_TOOL_REGISTRY_H_
#define CHROME_BROWSER_FLUX_AGENT_TOOL_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/providers/llm_provider.h"

namespace content {
class WebContents;
}

namespace flux {

class FluxAgentService;
class PageContext;

// Context handed to a tool for one invocation.
struct ToolContext {
  raw_ptr<content::WebContents> web_contents;
  raw_ptr<PageContext> page;
  std::string run_id;
  mojom::WriteScope scope;
};

// A single capability the model can invoke.
class Tool {
 public:
  using ResultCallback = base::OnceCallback<void(ToolResult)>;

  virtual ~Tool() = default;

  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual base::DictValue InputSchema() const = 0;

  // The strongest effect this tool can have. The runner compares this against
  // the task's declared WriteScope and gates on approval when it exceeds it.
  //
  // This is the mechanism that turns write-safety from a promise in marketing
  // copy into something the system actually enforces: a kReadOnly task
  // physically cannot invoke a kSend tool without a human clicking approve.
  virtual mojom::WriteScope RequiredScope() const = 0;

  // Plain-language description of what invoking with `input` will do, shown in
  // the approval prompt. Must be specific - "Send email to 12 recipients", not
  // "Perform an action".
  virtual std::string DescribeEffect(const base::DictValue& input) const = 0;

  // True when this tool acts on a web page, and so needs the run to have a tab.
  //
  // The tab is opened on first demand rather than when the run starts. Opening
  // it up front meant every run took over the user's window with an about:blank
  // tab before the first model turn - including runs that only call a connector
  // or write a file, and including one that ended immediately without ever
  // browsing. A tab is something the user notices, so it should appear when the
  // agent actually has somewhere to go.
  // Defined out of line: chromium-style's find-bad-constructs plugin rejects a
  // virtual method with a non-empty body declared inline in a header, and it
  // is an error under /WX. `{ return false; }` is non-empty.
  virtual bool NeedsPage() const;

  virtual void Run(const ToolContext& context,
                   base::DictValue input,
                   ResultCallback callback) = 0;
};

class ToolRegistry {
 public:
  ToolRegistry();
  ~ToolRegistry();

  ToolRegistry(const ToolRegistry&) = delete;
  ToolRegistry& operator=(const ToolRegistry&) = delete;

  void Register(std::unique_ptr<Tool> tool);
  // `service` is where the plan and artifact tools write. It owns this
  // registry, so it outlives every tool in it.
  void RegisterBuiltins(FluxAgentService* service);

  Tool* Get(const std::string& name) const;

  // Tool definitions the model is allowed to see for a task at `scope`.
  //
  // Tools above the task's scope are withheld entirely rather than offered and
  // then refused. A model that cannot see a send tool does not spend turns
  // trying to use it, and cannot argue for it.
  std::vector<ToolDefinition> DefinitionsForScope(mojom::WriteScope scope) const;

 private:
  std::map<std::string, std::unique_ptr<Tool>> tools_;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_TOOL_REGISTRY_H_
