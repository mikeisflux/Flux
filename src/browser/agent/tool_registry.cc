// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/tool_registry.h"

#include <utility>

#include "chrome/browser/flux/agent/tools/browser_tools.h"
#include "chrome/browser/flux/agent/tools/connector_tools.h"
#include "chrome/browser/flux/agent/tools/file_tools.h"
#include "chrome/browser/flux/agent/tools/plan_tools.h"

namespace flux {
namespace {

// Ordering of WriteScope by severity, so "does this tool exceed the task's
// scope" is a simple comparison.
int ScopeRank(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return 0;
    case mojom::WriteScope::kDraft:    return 1;
    case mojom::WriteScope::kSend:     return 2;
    case mojom::WriteScope::kPurchase: return 3;
  }
  return 3;
}

}  // namespace

bool Tool::NeedsPage() const {
  return false;
}

ToolRegistry::ToolRegistry() = default;
ToolRegistry::~ToolRegistry() = default;

void ToolRegistry::Register(std::unique_ptr<Tool> tool) {
  const std::string name = tool->name();
  tools_[name] = std::move(tool);
}

void ToolRegistry::RegisterBuiltins(FluxAgentService* service) {
  RegisterBrowserTools(this);
  RegisterConnectorTools(this);
  RegisterPlanTools(this, service);
  RegisterFileTools(this, service);
}

Tool* ToolRegistry::Get(const std::string& name) const {
  auto it = tools_.find(name);
  return it == tools_.end() ? nullptr : it->second.get();
}

std::vector<ToolDefinition> ToolRegistry::DefinitionsForScope(
    mojom::WriteScope scope) const {
  std::vector<ToolDefinition> out;
  for (const auto& [name, tool] : tools_) {
    if (ScopeRank(tool->RequiredScope()) > ScopeRank(scope))
      continue;
    ToolDefinition def;
    def.name = name;
    def.description = tool->description();
    def.input_schema = tool->InputSchema();
    out.push_back(std::move(def));
  }
  return out;
}

}  // namespace flux
