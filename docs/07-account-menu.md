# Flux — Account Menu (and the browser surface)

Opened from the avatar in the titlebar's right cluster. Dropdown ~320px wide,
right-aligned under the avatar, white, radius ~12px, soft shadow. Four labeled
groups plus an ungrouped footer, separated by hairline dividers.

## Contents

| Group | Item | Icon | Notes |
|---|---|---|---|
| **Profiles** | Mike Wheeler | avatar | active, right-aligned ✓ |
| | Add profile | `+` | |
| **Browser** | Bookmarks | book | |
| | History | clock-rewind | |
| | Downloads | download | |
| | Passwords | key | |
| | Extensions | puzzle | **`›` submenu chevron** |
| **Community** | Refer a Friend | envelope | |
| | Feedback & Bugs | megaphone | |
| | Join Slack Community | people | |
| **Agent** | Billing & Credits | credit-card | |
| *(footer)* | Settings | gear | |
| | Sign out | logout | **red text** — only destructive styling in the product |

## Why this screen matters

### 1. The full browser is still there — it was just hidden

Every prior screen showed an *agent console* with a vestigial tab strip. The
sidebar has no bookmarks, no history, no downloads. It was reasonable to
wonder whether Polar had stripped Chromium's browser surface down to a shell.

It has not. **Bookmarks, History, Downloads, Passwords, and Extensions are all
present** — relocated from Chrome's `⋮` menu into the avatar menu. Polar is a
complete browser whose browsing chrome has been demoted, not deleted.

This resolves a real design question for Flux: users still need the browser to
be a browser. The agent is additive.

### 2. Chromium profiles — and what they mean for parallel agents

`Profiles` / `Add profile` is standard Chromium multi-profile support: separate
cookie jars, separate logged-in identities, separate storage.

For an agentic browser this is **infrastructure, not a convenience feature**.
The launch post's claim — *"run ten of those at the same time"* — requires ten
concurrent sessions that do not collide over cookies, CSRF tokens, or
single-session-per-account services. Profiles are the natural isolation unit.

Nothing in the captured UI connects profiles to task runs, so it is unclear
whether Polar actually uses them this way. **[FLUX]** Bind runs to profiles
explicitly: a task declares which identity it runs as, parallel runs get
isolated contexts by default, and the run view shows which profile is acting.
Two agents silently sharing one cookie jar is a data-leak and a
logged-out-mid-run bug waiting to happen.

### 3. `Billing & Credits` — the first pricing signal in the entire product

Filed under an **`Agent`** group of its own. Nothing in Templates, Workflows,
Connectors, or Customize mentions cost. This is the only place the business
model surfaces, and it says: **agent work is metered in credits.**

That has direct product consequences the captured UI never addresses:

- A template card advertises an outcome but not what it costs to produce.
- A scheduled workflow spends credits unattended, forever, with no visible
  budget or cap.
- *"Every row re-checked overnight"* across a 500-row sheet is a very
  different bill from a one-off brief, and the cards look identical.

**[FLUX]** This is the strongest argument for the cost/runtime estimate badge
already specced in `01-ui-teardown.md`. Add to it: a per-workflow credit
budget with a hard stop, and a projected monthly spend shown at the moment a
task is promoted to a schedule. Unattended automation that silently bills is
the fastest way to lose a user's trust.

### 4. Extensions support — and an honest constraint for Flux

The `Extensions ›` submenu implies Chrome extension support.

**This is the one place where Flux's Electron foundation is genuinely weaker
than a native Chromium fork, and it should be stated plainly rather than
glossed.** Electron can load unpacked extensions and supports a useful subset
of the `chrome.*` APIs (devtools, some `storage`, basic content scripts), but
it is **not** a full extension host: no Chrome Web Store install flow, no
`chrome.tabs` parity, no MV3 service-worker fidelity, and no guarantee that an
arbitrary store extension will run.

Options, in order of cost:

1. **Ship without extension support.** Honest, fastest. Most of what users
   install extensions for (ad blocking, password management) can be provided
   natively.
2. **Support a curated allowlist** of extensions verified to work under
   Electron's subset.
3. **Move to a native Chromium fork** (or an embedder like CEF) if full
   extension parity turns out to be a real requirement.

The recommendation is (1) for v1, with (2) as a follow-on. Do not promise
extension support the foundation cannot deliver — this is exactly the kind of
gap that is cheap to disclose now and expensive to discover later.

### 5. Community as a growth loop

`Refer a Friend`, `Feedback & Bugs`, `Join Slack Community` — an early-stage
distribution and feedback pattern, given equal billing with browser features.
Worth noting as product strategy; not load-bearing for the build.

## Design notes

- **`Sign out` in red is the only destructive-action styling anywhere** in the
  captured product. Notable given how much genuinely destructive capability the
  agent has — sending emails, submitting forms, staging payments — none of
  which is color-coded anywhere. The riskiest actions in the product are
  styled more calmly than signing out.
- Group labels (`Profiles`, `Browser`, `Community`, `Agent`) are ~11px
  uppercase gray, same treatment as `AVAILABLE` on the Connectors tab.
- `Settings` sits outside any group, implying it is the catch-all for
  everything the four groups don't cover. Not captured.

## Still unseen

- ~~`Settings`~~ — captured, see `08-settings.md`. Notably it contains **no
  model picker** and no agent-behavior tuning.
- The `Extensions ›` submenu.
- `Billing & Credits` — pricing tiers, credit costs per run, budget controls.
- Whether profiles are bound to task runs in any way. (Settings confirms
  profiles are explicitly "isolated" and can each open their own window, but
  still shows no link between a profile and a task run.)
