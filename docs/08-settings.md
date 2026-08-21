# Flux — Settings

Settings opens as a **separate native OS window**, not an in-app tab or route.
It uses system chrome (native Windows titlebar, Fluent-style controls,
segmented pickers, blue toggles) rather than the app's own black-and-white
design language.

That is a deliberate split — *app surfaces get the custom design system,
machine configuration gets native chrome* — and it diverges from Chrome, which
renders settings as a page at `chrome://settings`. It's defensible: settings
here are OS-level concerns (profiles, download paths, default browser), and
native widgets get accessibility and platform conventions for free.

Two nav items: **Profiles**, **General**.

---

## 1. Profiles

> **Profiles** — Create, select, and manage **isolated** browsing profiles.

`Add profile` button, top-right. Below it, a card per profile: avatar,
name, and a selection radio.

### `SELECTED PROFILE` — actions

| Row | Description | Control |
|---|---|---|
| Open new window | Open a new window using this profile. | `Open` |
| Profile name | Change how this profile is labeled. | `Rename` |
| Import browsing data | Bring **bookmarks, extensions, and history** from another browser. | `Import…` |
| Delete profile & data | Remove this profile and its browsing data from Polar. | `Delete` *(disabled — last remaining profile)* |

Two notes: the word **"isolated"** is used explicitly in the subtitle, and
`Import browsing data` names **extensions** among the importable items —
independent confirmation that extension support is real, not vestigial.

### `Settings for Mike Wheeler` — **per-profile** configuration

> These settings apply only to the selected profile.

| Group | Setting | Control | State |
|---|---|---|---|
| **Appearance** | Theme — *System follows your device appearance.* | segmented: `System` / `Light` / `Dark` | System |
| **Bookmarks** | Show bookmarks bar — *Display the bookmarks bar under the toolbar.* | toggle | off |
| **Downloads** | Ask before each download — *Choose where to save every file.* | toggle | off |
| | Download location — `~\Downloads` | `Change` | |
| **Notifications** | **Agent completion** — *Notify whenever the agent finishes a task.* | toggle | **on** |
| **Keyboard shortcuts** | **Command panel** — *Open the **tab-scoped** command panel.* | `Ctrl+K` | |

**Theme has a Dark mode.** Every screen captured so far has been light. Flux
must build both palettes from the start — retrofitting dark onto a
finished light-only design system is significantly more expensive.

**`Agent completion` is the only agent-specific notification**, and it ships
**on**. Correct default: tasks are long-running and unattended, so the OS
notification is the primary completion channel. Note what is *absent* —
there is no notification for an agent that is **blocked**, **failed**, or
**waiting on approval**. A task that stalls halfway through, at 3am, on a
schedule, tells the user nothing.

**[FLUX]** Notify on four events, each independently toggleable: completed,
failed, blocked-on-approval, and budget-threshold-reached. The approval one is
the load-bearing case — an agent paused waiting for a human is worthless if
the human isn't told.

---

## 2. General

| Group | Setting | Control |
|---|---|---|
| — | Default browser — *Polar is your default web browser.* | `Default` *(disabled — already default)* |
| **Agent** | **Parallel tasks** — *Sized to this PC's memory, shared across running tasks.* | segmented: `Auto` / `Lower` / `Higher` |
| **Help & Community** | Feedback & bugs | `Send Feedback` |
| | Community — *Join the Polar Slack to connect with other users.* | `Join Slack` |

---

## 3. `Parallel tasks` — the most important setting in the product

> **Parallel tasks** — Sized to this PC's memory, shared across running tasks.

This single line answers the question the launch post raised and no other
screen addressed: *"you can run ten of those at the same time."*

**Parallelism is local and RAM-bound.** Tasks execute on the user's own
machine, in local browser contexts, sharing that machine's memory. This is not
a cloud fleet. Three consequences follow directly:

1. **Concurrency is capped by hardware, not by plan.** `Auto` sizes the pool to
   available memory. A 16 GB laptop and a 64 GB workstation get materially
   different products from identical software.
2. **Each parallel task is a real browser context** — on the order of hundreds
   of MB with a heavy page loaded. Ten concurrent agents on a typical laptop is
   genuinely demanding, and "ten at once" is a best-case number on good
   hardware, not a product guarantee.
3. **The machine must stay awake.** A scheduled 7am workflow does not run if
   the laptop is closed. Nothing in the Workflows UI mentions this, and it is
   the single most likely cause of a schedule silently not firing.

The `Auto / Lower / Higher` control is well-judged — it exposes a real
resource tradeoff without asking the user to pick a thread count. Copying that.

**[FLUX]**
- Show the **resolved concurrency number** next to the setting ("Auto — 4
  concurrent tasks on this machine"), not just the abstraction. Users
  troubleshooting a slow queue need the actual value.
- Surface **queue depth** in the Runs sidebar: tasks beyond the cap are
  queued, not running, and that distinction must be visible.
- **Warn at schedule time** if a workflow is set to fire when the machine is
  typically asleep, and record "missed — machine unavailable" in run history
  rather than showing nothing.
- Consider an optional **cloud execution** tier for scheduled workflows
  specifically. Local-first is the right default for privacy and for
  browser-session reuse, but "runs only when your laptop is open" is a hard
  ceiling on the scheduled-automation promise, which is 40% of the catalog.

---

## 4. `Ctrl+K` — the tab-scoped command panel

> **Command panel** — Open the **tab-scoped** command panel. `Ctrl+K`

This confirms the unified `/command` namespace inferred in
`06-customize-tab.md` from skill commands (`data-create-chart`) and the
Workflows empty state (*"trigger it anytime with /command"*). `Ctrl+K` is how
that namespace is reached, and it is almost certainly what the magnifier icon
in the titlebar opens.

**"Tab-scoped" is the operative word.** The command panel runs *against the
current tab* — so `/data-create-chart` presumably acts on the page in front of
you. That reframes the whole product: it is not only a task console with a
browser attached, it is a browser where any page can be handed to a skill in
one keystroke.

That makes the naming inconsistency found in `06` more costly than it first
appeared. A `Ctrl+K` palette is a *typing* interface — its value depends on
commands being guessable. With `dashboard-building`, `data-create-chart`, and
`data-warehouse-context` each derived by a different rule, users will fuzzy-search
rather than type, and 117 skills plus every workflow in one flat namespace makes
that search noisy.

**[FLUX]** Generate commands to one convention, namespace them explicitly
(`skill:` / `flow:`), and make the palette rank by current-page relevance —
tab-scoped means the page is the strongest ranking signal available, and the
reference does not appear to use it.

---

## 5. Structure notes

- **Per-profile vs. global is a real split.** Theme, bookmarks bar, downloads,
  notifications, and shortcuts are per-profile. Only default-browser, parallel
  tasks, and help are global. Per-profile theme is unusual — Chrome themes are
  per-profile but appearance settings generally aren't — and it fits a product
  where profiles are work-identity boundaries.
- `Delete` is correctly disabled for the only remaining profile.
- `Default` is disabled because Polar already holds the default-browser role.
- Settings contains **no model picker**, no agent-behavior tuning, and no
  credit/budget controls. Combined with `Billing & Credits` living in the
  account menu, cost governance exists in neither place.

**[FLUX]** Add an `Agent` settings section covering model routing
(Claude/OpenAI, per-task override), default write-scope policy (what the agent
may do without asking), and credit budgets with hard caps.

## 6. Still unseen

- The `Billing & Credits` screen — pricing tiers, per-run costs, budget controls.
- `Add profile` flow, and whether profiles can be bound to specific tasks.
- The `Extensions ›` submenu.
- The task **run view**, the **New task** composer, and the populated
  **Workflows** list.
