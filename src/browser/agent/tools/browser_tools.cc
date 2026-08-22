// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/flux/agent/page_context.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/agent/tools/browser_tools.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace flux {
namespace {

base::DictValue StringProp(const std::string& description) {
  base::DictValue d;
  d.Set("type", "string");
  d.Set("description", description);
  return d;
}

base::DictValue IntProp(const std::string& description) {
  base::DictValue d;
  d.Set("type", "integer");
  d.Set("description", description);
  return d;
}

base::DictValue ObjectSchema(base::DictValue properties,
                               std::vector<std::string> required) {
  base::DictValue schema;
  schema.Set("type", "object");
  schema.Set("properties", std::move(properties));
  base::ListValue req;
  for (auto& r : required)
    req.Append(std::move(r));
  schema.Set("required", std::move(req));
  return schema;
}

ToolResult Ok(const std::string& content) {
  ToolResult r;
  r.content = content;
  r.is_error = false;
  return r;
}

ToolResult Err(const std::string& message) {
  ToolResult r;
  r.content = message;
  r.is_error = true;
  return r;
}

// ---------------------------------------------------------------------------

class NavigateTool : public Tool {
 public:
  std::string name() const override { return "navigate"; }
  std::string description() const override {
    return "Navigate the browser to a URL and wait for the page to settle. "
           "Returns a snapshot of the loaded page.";
  }
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("url", StringProp("Absolute URL to load."));
    return ObjectSchema(std::move(props), {"url"});
  }
  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* url = input.FindString("url");
    return base::StrCat({"Open ", url ? *url : "(missing url)"});
  }

  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    const std::string* url_str = input.FindString("url");
    if (!url_str) {
      std::move(callback).Run(Err("Missing required parameter 'url'."));
      return;
    }
    GURL url(*url_str);
    // Refuse non-web schemes outright. file:// would give a remote model read
    // access to the local disk; chrome:// would expose privileged UI.
    if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
      std::move(callback).Run(
          Err("Only http and https URLs are allowed. Got: " + *url_str));
      return;
    }
    if (!ctx.web_contents) {
      std::move(callback).Run(Err("No active tab."));
      return;
    }

    ctx.web_contents->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

    ctx.page->CaptureWhenStable(base::BindOnce(
        [](ResultCallback cb, PageContext::Snapshot snapshot) {
          std::move(cb).Run(Ok(PageContext::Format(snapshot)));
        },
        std::move(callback)));
  }
};

class ReadPageTool : public Tool {
 public:
  std::string name() const override { return "read_page"; }
  std::string description() const override {
    return "Read the current page: its text content and every element you can "
           "interact with, each numbered so you can act on it.";
  }
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    return ObjectSchema({}, {});
  }
  std::string DescribeEffect(const base::DictValue&) const override {
    return "Read the current page";
  }
  void Run(const ToolContext& ctx,
           base::DictValue,
           ResultCallback callback) override {
    if (!ctx.page) {
      std::move(callback).Run(Err("No page context."));
      return;
    }
    ctx.page->CaptureWhenStable(base::BindOnce(
        [](ResultCallback cb, PageContext::Snapshot snapshot) {
          std::move(cb).Run(Ok(PageContext::Format(snapshot)));
        },
        std::move(callback)));
  }
};

class ClickTool : public Tool {
 public:
  std::string name() const override { return "click"; }
  std::string description() const override {
    return "Click an element by the node id shown in the page snapshot. Use "
           "read_page first to get ids; never guess one.";
  }
  // Clicking is not itself a write, but the runner escalates submit-like
  // targets to kSend before dispatch. See AgentRunner::RequiresApproval.
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("node_id", IntProp("Node id from the page snapshot."));
    return ObjectSchema(std::move(props), {"node_id"});
  }
  std::string DescribeEffect(const base::DictValue& input) const override {
    std::optional<int> id = input.FindInt("node_id");
    return base::StrCat({"Click element ", base::NumberToString(id.value_or(-1))});
  }
  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    std::optional<int> node_id = input.FindInt("node_id");
    if (!node_id) {
      std::move(callback).Run(Err("Missing required parameter 'node_id'."));
      return;
    }
    if (!ctx.page) {
      std::move(callback).Run(Err("No page context."));
      return;
    }
    ctx.page->ClickNode(*node_id, base::BindOnce(
        [](ResultCallback cb, PageContext* page, bool ok) {
          if (!ok) {
            std::move(cb).Run(Err(
                "No element with that node id, or it is not clickable. "
                "Call read_page again - the page may have changed."));
            return;
          }
          page->CaptureWhenStable(base::BindOnce(
              [](ResultCallback inner, PageContext::Snapshot s) {
                std::move(inner).Run(Ok(PageContext::Format(s)));
              },
              std::move(cb)));
        },
        std::move(callback), ctx.page));
  }
};

class TypeTool : public Tool {
 public:
  std::string name() const override { return "type"; }
  std::string description() const override {
    return "Type text into a text field identified by node id. Replaces any "
           "existing value.";
  }
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("node_id", IntProp("Node id of the text field."));
    props.Set("text", StringProp("Text to enter."));
    return ObjectSchema(std::move(props), {"node_id", "text"});
  }
  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* text = input.FindString("text");
    return base::StrCat({"Type \"", text ? *text : "", "\""});
  }
  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    std::optional<int> node_id = input.FindInt("node_id");
    const std::string* text = input.FindString("text");
    if (!node_id || !text) {
      std::move(callback).Run(Err("Requires 'node_id' and 'text'."));
      return;
    }
    ctx.page->TypeIntoNode(*node_id, *text, base::BindOnce(
        [](ResultCallback cb, bool ok) {
          std::move(cb).Run(ok ? Ok("Text entered.")
                               : Err("That node is not a text field."));
        },
        std::move(callback)));
  }
};

class ExtractTool : public Tool {
 public:
  std::string name() const override { return "extract"; }
  std::string description() const override {
    return "Extract structured rows from the current page. Give the field "
           "names you want; returns JSON. Use this instead of reading the "
           "whole page when you need tabular or list data.";
  }
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    base::DictValue fields;
    fields.Set("type", "array");
    base::DictValue items;
    items.Set("type", "string");
    fields.Set("items", std::move(items));
    fields.Set("description", "Field names to extract for each row.");

    base::DictValue props;
    props.Set("fields", std::move(fields));
    return ObjectSchema(std::move(props), {"fields"});
  }
  std::string DescribeEffect(const base::DictValue&) const override {
    return "Extract structured data from the page";
  }
  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    // Extraction runs against the accessibility snapshot rather than the DOM,
    // so it survives markup changes that would break a selector-based scraper.
    ctx.page->CaptureWhenStable(base::BindOnce(
        [](ResultCallback cb, base::DictValue in, PageContext::Snapshot s) {
          std::move(cb).Run(Ok(PageContext::FormatForExtraction(s, in)));
        },
        std::move(callback), std::move(input)));
  }
};

class SubmitTool : public Tool {
 public:
  std::string name() const override { return "submit"; }
  std::string description() const override {
    return "Submit a form. This transmits data and cannot be undone.";
  }
  // The scope boundary. A kReadOnly or kDraft task never sees this tool.
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kSend;
  }
  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("node_id", IntProp("Node id of the form or submit control."));
    return ObjectSchema(std::move(props), {"node_id"});
  }
  std::string DescribeEffect(const base::DictValue& input) const override {
    std::optional<int> id = input.FindInt("node_id");
    return base::StrCat({"Submit the form at element ",
                         base::NumberToString(id.value_or(-1)),
                         " - this transmits data and cannot be undone"});
  }
  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    std::optional<int> node_id = input.FindInt("node_id");
    if (!node_id) {
      std::move(callback).Run(Err("Missing 'node_id'."));
      return;
    }
    ctx.page->SubmitForm(*node_id, base::BindOnce(
        [](ResultCallback cb, bool ok) {
          std::move(cb).Run(ok ? Ok("Form submitted.")
                               : Err("Could not submit that element."));
        },
        std::move(callback)));
  }
};

class WaitForTool : public Tool {
 public:
  std::string name() const override { return "wait_for"; }
  std::string description() const override {
    return "Wait until text appears on the page, or until a timeout. Use this "
           "after an action that triggers loading rather than guessing.";
  }
  mojom::WriteScope RequiredScope() const override {
    return mojom::WriteScope::kReadOnly;
  }
  base::DictValue InputSchema() const override {
    base::DictValue props;
    props.Set("text", StringProp("Text to wait for."));
    props.Set("timeout_seconds", IntProp("Max seconds to wait (default 30)."));
    return ObjectSchema(std::move(props), {"text"});
  }
  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* t = input.FindString("text");
    return base::StrCat({"Wait for \"", t ? *t : "", "\" to appear"});
  }
  void Run(const ToolContext& ctx,
           base::DictValue input,
           ResultCallback callback) override {
    const std::string* text = input.FindString("text");
    const int timeout = input.FindInt("timeout_seconds").value_or(30);
    ctx.page->WaitForText(*text, base::Seconds(timeout), base::BindOnce(
        [](ResultCallback cb, bool found) {
          std::move(cb).Run(found
              ? Ok("Text appeared.")
              : Err("Timed out. The page may not be loading what you expect - "
                    "call read_page to see the current state."));
        },
        std::move(callback)));
  }
};

}  // namespace

void RegisterBrowserTools(ToolRegistry* registry) {
  registry->Register(std::make_unique<NavigateTool>());
  registry->Register(std::make_unique<ReadPageTool>());
  registry->Register(std::make_unique<ClickTool>());
  registry->Register(std::make_unique<TypeTool>());
  registry->Register(std::make_unique<ExtractTool>());
  registry->Register(std::make_unique<WaitForTool>());
  registry->Register(std::make_unique<SubmitTool>());
}

}  // namespace flux
