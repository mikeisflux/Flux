# Flux — UI Teardown (from Polar reference screenshots)

Observational notes taken from 6 reference screenshots of Polar (Recursive
Intelligence, July 2026). This is a record of *what is on screen* and how it is
laid out. Design decisions where Flux deliberately diverges are marked **[FLUX]**.

Reference resolution: 2000 × 1191 CSS px (window is not maximized; Windows-style
controls). All measurements below are read off that frame and expressed as the
app's own layout units.

---

## 1. Window shell

Frameless window with a fully custom titlebar. Three horizontal bands:

```
┌──────────────────────────────────────────────────────────────────────┐
│ TITLEBAR  (h ≈ 44px, draggable)                                      │
├──────────┬───────────────────────────────────────────────────────────┤
│ SIDEBAR  │  CONTENT                                                  │
│ (305px)  │  (fills remainder)                                        │
│          │                                                           │
└──────────┴───────────────────────────────────────────────────────────┘
```

The sidebar starts at the very top of the window — the titlebar does **not**
span the full width. The tab strip begins to the right of the sidebar's
gutter, at x ≈ 305. The sidebar region of the titlebar is empty (pure drag
area) except for the two icon buttons at far left.

**The toolbar is there.** Most reference captures show no omnibox, which reads
as "this product removed the address bar" — it did not. The toolbar row (back,
forward, reload, omnibox, extensions) and the bookmarks bar appear below the
titlebar as soon as focus is in the tab rather than in the console. They are
hidden while the console has focus because the console is not a web page and
has no address. This is a focus-dependent band, not a removed one, and Flux
keeps it: an agent browser you cannot type a URL into is a worse browser, and
the account menu in `07-account-menu.md` is an *addition* to the toolbar, not a
replacement for it.

### 1.1 Titlebar, left cluster (x ≈ 232–290)
| Element | Icon | Notes |
|---|---|---|
| Search | magnifier, 18px stroke | Opens command palette / global search |
| Sidebar toggle | panel-left, 18px stroke | Collapses the 305px sidebar |

Both are ghost buttons: no border, no fill at rest, subtle rounded hover fill.
They sit at the sidebar's right edge, not its left — the ~230px to their left
is empty drag space. **[FLUX]** We anchor them at x=16 instead; the empty
230px is wasted and looks like a rendering bug on smaller windows.

### 1.2 Tab strip (x ≈ 305 → 590)
- One tab shown: dark circular favicon + label `New Tab`.
- Tab is ~250px wide, no visible border or background at rest — tabs render as
  plain text rows until hovered/selected.
- On hover, an `×` close affordance appears at the tab's right edge (visible in
  screenshots 3 and 4, absent in 1 and 2 → hover-only, not persistent).
- A thin vertical divider `|` at x ≈ 583 separates tabs from the new-tab button.
- `+` new-tab button at x ≈ 602.

Tabs are top-mounted and horizontal (Chrome-like), **not** a vertical tab list
in the sidebar — worth noting since the sidebar is otherwise Arc-shaped.

### 1.3 Titlebar, right cluster
| Element | Notes |
|---|---|
| Avatar | 28px circle, magenta/pink fill, white initials — opens the account menu (see `07-account-menu.md`; all browser surface — bookmarks, history, downloads, passwords, extensions — lives there) |
| Minimize | Windows-style `—` |
| Maximize | Windows-style `▢` |
| Close | Windows-style `×` |

Standard Windows control metrics (46 × 32 hit targets, close turns red on
hover). **[FLUX]** Must be platform-conditional: macOS gets inset traffic
lights on the left and no right-side cluster.

---

## 2. Sidebar (305px)

Five nav items, top-aligned, starting y ≈ 60. Each row: 20px stroke icon +
15px label, ~14px vertical padding, full-width rounded-8px hit area.

| # | Label | Icon | Destination |
|---|---|---|---|
| 1 | New task | compose / pencil-square | Task composer |
| 2 | Templates | grid of 4 squares | Template library |
| 3 | Workflows | repeat / loop arrows | Saved scheduled runs |
| 4 | Connectors | link / chain | Third-party account auth |
| 5 | Customize | cube / box | Preferences, voice, personalization |

Active state = light gray (`#f1f1f0`-ish) rounded-8px fill spanning the
sidebar's inner width with ~8px side inset. Text stays near-black; there is
**no** accent color, no left indicator bar, no icon tinting.

The sidebar has no section headers, no history list, no bookmarks, no open-tab
list, and no account row at the bottom. It is a pure 5-item app switcher.

**[FLUX] Additions:** a live **Runs** section below Customize showing currently
executing agents with inline progress, and a pinned **Approvals** row that
badges when an agent is blocked waiting on the human. The reference has
nowhere to surface either — a running task is invisible unless you navigate to
it.

---

## 3. Screen: New Tab

Two states, and the reference shows both.

### 3.0 The composer — the default

Once there is any run history, the new tab is a task box, centered in the
content column at ~790px:

- **Heading** `What can I do for you?` — ~40px, light weight, centered.
- **Composer card**: 1px border, radius ~16px, white. A borderless textarea
  with placeholder `Describe your task, / for commands, @ for context` — which
  is where slash-commands and an `@` context picker are advertised, the only
  place in the product either appears outside the Workflows empty state.
- **Bottom bar of the card**: a bare `Medium ⌄` dropdown at the left; at the
  right a display/watch toggle, a paperclip, and a **filled blue circular send
  button**.
- **Suggestion chips** below, centered: `Triage my inbox` (Gmail mark),
  `Prep my day` (calendar mark), `Research a topic` (magnifier), `More`.

**[FLUX]** `Medium` is unlabelled in the reference, which is a strange thing to
leave unexplained when it is the only control on the screen that decides what a
run costs. Flux makes it a budget — Quick / Medium / Thorough set the output
token ceiling and the credit ceiling, and the browser process fails the run
closed at the limit rather than billing on.

The send button is the **only saturated pixel in the product chrome**, and the
one deliberate exception to the no-accent rule in §5. It works precisely
because everything around it is gray.

### 3.1 First run: try an example — screenshot 1

Content column is centered in the content area, ~672px wide.

- **Heading** `Try an example` — ~40px, light weight (300–400), near-black,
  centered, at y ≈ 271.
- **Three cards**, stacked vertically, 16px gap, starting y ≈ 330.
- **Footer row** at y ≈ 725, centered, 13px gray:
  `⇄ Shuffle` · `I want to try myself →`

### Card anatomy (example cards)
```
┌────────────────────────────────────────────────────────┐
│  [icon]   Title in near-black, 15px semibold           │
│   24px    Two-line description, 13px, gray             │
└────────────────────────────────────────────────────────┘
```
- Border 1px `#e8e8e6`, radius 12px, white fill, no shadow.
- Icon column ~56px wide, icon vertically centered against the title line.
- Padding ~20px 24px.

### The three seeded examples
| Icon | Title | Description |
|---|---|---|
| Gmail | Draft replies to emails waiting on me | Finds every email waiting on you in Gmail and saves a ready reply as a draft. Nothing sends without you. |
| Google Docs | Turn any list page into a ranked sheet | Point it at any page that lists things. It opens every entry, fills in the details, and ranks them in a clean sheet. |
| Sparkle (generic) | Build my conference hit list | Finds your next event, researches the exhibitors and speakers, and builds a hit list with booth numbers and talking points. |

Note the copy discipline — every description states the **artifact produced**,
not the process. "saves a ready reply as a draft", "ranks them in a clean
sheet", "builds a hit list with booth numbers". Flux copy follows the same rule.

`Shuffle` re-rolls the three examples from the wider catalog.

---

## 4. Screen: Templates — screenshots 2–6

Introduces a **second column** between sidebar and content:

```
│ SIDEBAR │ TEMPLATE NAV │ CONTENT (scrolls)                │
│ 305px   │ ~240px       │ fills                            │
```

### 4.1 Template nav column
- Header `Templates`, 17px semibold, y ≈ 73.
- Two items: `Tasks` (active — gray rounded pill, full column width) and
  `Skills`.
- The distinction: **Tasks** are runnable prompts; **Skills** are reusable
  capabilities a task can call. Skills screen was not captured.

### 4.2 Content header
- H1 `Tasks`, ~28px semibold.
- Subtitle, 14px gray: *"Browse what Polar can do. Use a task as a starting
  point, or save it as a workflow that runs on a schedule."* — this is the
  clearest statement in the whole UI of the task→workflow promotion model.
- **Search field**: full-width, ~44px tall, radius 10px, 1px border, magnifier
  at left. Placeholder `Search tasks, sites, roles...` — note it advertises
  searching by **site** and **role**, not just task text.

### 4.3 Filter row 1 — categories
Pills, ~34px tall, radius 999px, 12px gap:
`All` (active: black fill, white text) · Sales · Recruiting · Marketing · Data ·
Research · Ops · Engineering · Docs · Personal · Monitoring

Inactive pills: white fill, 1px `#e5e5e3` border, near-black text.

### 4.4 Filter row 2 — facets
- `For Everyone ⌄` — dropdown, presumably a role/persona filter that re-ranks
  the catalog.
- `⟳ Scheduled` — toggle chip filtering to templates that ship with a schedule.

### 4.5 Sections
When `All` is selected the page renders `Featured` first, then one section per
category in fixed order: Sales, Recruiting, Marketing, Data, Research, Ops,
Engineering, Docs, Personal, Monitoring.

Section header: category icon + name + **count** + right-aligned `See all →`.
Featured has no count and no See-all.

Observed counts: Sales 32 · Recruiting 21 · Marketing 26 · Data 25 ·
Research 24 · Ops 31 · Engineering 21 · Docs (cut off) · Personal 26 ·
Monitoring 23. Sections show the first 6; Featured shows 12.

### 4.6 Template card anatomy
3-column grid, ~24px gutters, cards ~360×~145px.

```
┌──────────────────────────────────────────┐
│ Title, 15px semibold, wraps to 2 lines   │
│                                          │
│ Outcome description, 13px gray, 2 lines  │
│                                          │
│ [icon][icon][icon] [+1]  [⟳ schedule]    │
└──────────────────────────────────────────┘
```
- **Connector icons** (16px, full-color brand marks) show which integrations
  the task touches: LinkedIn, Gmail, Google Docs, Google Sheets, Slack, Google
  Calendar, Instagram, TikTok, X, Reddit, QuickBooks, Monday.com, Gong, G2,
  Crunchbase, Yahoo/YC, Resy, OpenTable, Zillow, Greenhouse.
- Overflow renders as a `+1` gray chip.
- **Schedule chip**: `⟳ Every day at 7am` — 12px, gray text, light pill,
  rounded, right of the icon row. Only present on scheduled templates.
- Cards with neither connectors nor schedule (e.g. *Deep-dive every company in
  a category into a market map*) leave the footer row empty — they run on the
  open web only.
- Card bottom-aligns its footer row, so cards in a row stay flush regardless of
  title wrap.

**[FLUX] Additions to the card:** an estimated **runtime + token cost** badge,
and a `Trust` marker showing whether the task is read-only, writes drafts, or
performs irreversible sends. The reference gives you no way to tell "drafts a
reply" from "sends a reply" until you read the description prose.

---

## 5. Design language

| Token | Value (read from screenshots) |
|---|---|
| Page background | pure white `#ffffff` |
| Card / input border | `#e8e8e6` 1px |
| Active pill fill | `#111111` (black), white text |
| Hover / active nav fill | `#f1f1f0` |
| Primary text | `#1a1a1a` |
| Secondary text | `#6b6b68` |
| Card radius | 12px |
| Pill radius | 999px |
| Input radius | 10px |
| Nav item radius | 8px |

**Typeface** is a geometric humanist sans with a distinctive single-storey `a`
and a curved-tail `y` — reads as Gilroy / Poppins / Object Sans family, not
Inter and not the system stack. Headings run light; body runs regular.

There is **no accent color anywhere in the product chrome**. The only color on
screen comes from third-party brand icons. Selection and emphasis are carried
entirely by black fills and gray tints. This is a deliberate, and effective,
choice — the connector icons become the only saturated pixels, so the eye goes
straight to "what does this task touch". **[FLUX] keeps this rule.**

Density is low: generous padding, 2-line descriptions never truncated, large
click targets. Nothing is more than two levels deep.

---

## 6. What the reference does *not* show

Captured screens are the empty state and the template library only. Not seen,
and therefore designed from scratch in Flux:

- The task **run view** — how a live agent's progress, browser actions, and
  intermediate output are displayed.
- **Parallel runs** — the launch post claims "run ten of those at the same
  time" but no UI for it appears.
- The **Skills** screen.
- **Connectors** — the auth/consent flow.
- **Customize** — preferences, voice/tone personalization.
- Any **approval / confirmation** surface for irreversible actions.
- History, results archive, or run logs.
- Settings for model choice — no model picker is visible anywhere, implying a
  single fixed model. **[FLUX] exposes per-task Claude/OpenAI routing.**
