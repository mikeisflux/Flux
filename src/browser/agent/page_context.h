// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_
#define CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {
class WebContents;
}

namespace flux {

// A page element the agent can act on, addressed by a stable id rather than a
// CSS selector. Selectors are the standard approach and they are why browser
// agents break on every site redesign; node ids come from the accessibility
// tree the browser already maintains for screen readers.
struct InteractiveNode {
  int32_t node_id;
  std::string role;         // button, textbox, link, checkbox...
  std::string name;         // accessible name, i.e. what a human would read
  std::string value;
  bool is_focusable = false;
  bool is_offscreen = false;
  gfx::Rect bounds;
};

// Serializes a live page into something a model can reason about.
//
// This is the single highest-leverage component in the agent. Feeding raw DOM
// wastes enormous context on markup the model does not need and still misses
// what is actually visible; feeding screenshots costs vision tokens and loses
// exact text. The accessibility tree is the browser's own semantic model of
// the page — it is what the page *means*, already computed, already handling
// shadow DOM, iframes and ARIA.
//
// Being inside Chromium rather than driving it from outside is what makes this
// cheap: no CDP round-trips, direct access to the AXTree the renderer already
// built.
class PageContext {
 public:
  struct Snapshot {
    std::string url;
    std::string title;
    // Readable text, already stripped of chrome, nav and boilerplate.
    std::string content;
    std::vector<InteractiveNode> interactive;
    // True when the page is still loading; the agent should wait rather than
    // act on a half-rendered tree.
    bool is_stable = false;
  };

  using SnapshotCallback = base::OnceCallback<void(Snapshot)>;

  explicit PageContext(content::WebContents* web_contents);
  ~PageContext();

  PageContext(const PageContext&) = delete;
  PageContext& operator=(const PageContext&) = delete;

  // Captures once the page is quiescent (load complete plus a short settle for
  // client-rendered content).
  void CaptureWhenStable(SnapshotCallback callback);

  // Renders a snapshot into the compact text form given to the model.
  // Interactive elements are numbered so the model can say "click 14" rather
  // than emitting a selector it guessed.
  static std::string Format(const Snapshot& snapshot);

 private:
  void OnAccessibilityTreeReady(SnapshotCallback callback,
                                const ui::AXTreeUpdate& update);

  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtrFactory<PageContext> weak_factory_{this};
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_AGENT_PAGE_CONTEXT_H_
