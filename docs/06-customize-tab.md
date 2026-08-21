# Flux — Customize Tab (Instructions + Skills), and the Skill Schema

The single most useful capture for implementation: it exposes the **internal
structure of a skill**, which every other screen only showed as a card.

---

## 1. Customize › Instructions

Three-column shell: sidebar | `Customize` nav (`Instructions`, `Skills`) |
content. Same nav-column pattern as Templates.

- **H1** `Instructions`
- **Subtitle**, 2 lines:

  > Anything Polar should know on every task. E.g. context about you, how you
  > like things done, things to avoid. **Polar also saves useful things it
  > learns here.**

- **Textarea** — large (~900 × 500), radius 12px, 1px border, resize handle
  bottom-right. Placeholder: `e.g. "Keep answers short and direct."`
- **`Save`** button, bottom-right, **gray/disabled** while the field is empty.

### The important half-sentence

> *"Polar also saves useful things it learns here."*

This is a **bidirectional memory store**. It is not just a user-authored system
prompt — the agent writes back into the same buffer the user edits. That is a
real and unusual design choice:

- **Upside:** memory is fully transparent and directly editable. The user can
  read exactly what the agent believes about them and delete a line. Compare
  this to opaque vector-store memory, where the user cannot audit or correct it.
- **Risk:** the agent can overwrite or bloat the user's own instructions, and
  there is no visible provenance — nothing distinguishes "I wrote this" from
  "the agent learned this". No diff, no timestamp, no undo shown.

**[FLUX]** Keep the single transparent editable buffer — it is the right call —
but split it into two visually distinct regions: **Your instructions** (user
authored, agent read-only) and **What Flux has learned** (agent-authored, each
entry timestamped, with provenance to the run that created it, and individually
dismissable). Same transparency, no silent overwrite.

---

## 2. Customize › Skills

Adds a **fourth column** — the only four-column screen captured:

```
│ SIDEBAR │ Customize nav │ Skills list │ Detail pane │
│ 305px   │ ~240px        │ ~375px      │ fills       │
```

### Skills list column

- Header `Skills` + **ⓘ info icon**, then two buttons right-aligned:
  - **`⬆ Import`** — outlined. Skills are an importable file format.
  - **`New ⌄`** — black filled, with chevron → multiple creation paths.
- **`Suggested`** section, with `See more →`. Three entries, plain text rows
  (no cards):
  - Dashboard design & building
  - Chart creation from data
  - Data warehouse context

  All three are **Data**-category skills. With `Your skills` empty, these are
  presumably ranked by recent activity or role, not fixed.
- **`Your skills`** → `None yet`.

So the 117-skill library in Templates is a **catalog to adopt from**; a user's
*active* skill set starts empty. "Add a skill to make it yours" from the
Templates subtitle is literally a copy-into-your-library action.

### Detail pane empty state

Layers icon (stacked sheets), heading `Skills`, body:

> Skills are know-how Polar automatically uses when it's relevant to a task.
> E.g. writing a Word doc or designing a poster.

Note the examples given — *writing a Word doc*, *designing a poster* — are
generic capability skills, not the enterprise skills the catalog is full of.

---

## 3. The Skill detail modal — **the schema**

Opened by clicking a skill. Modal ~790px wide, centered, radius ~16px,
internally scrollable, dimmed backdrop.

### Structure, top to bottom

| Region | Content |
|---|---|
| **Header** | 48px rounded icon tile + skill name + one-line description |
| **Role chips** | `For analysts` `For founders` `For marketing` — outlined pills |
| **Works with** | Row of connector icons (here: Docs, Sheets) |
| **Instructions** | Bordered card holding the skill body as rendered markdown |
| **Action** | `+ Add to my skills` — black filled button |
| **Related skills** | Plain text links to sibling skills |

### Two things this settles

1. **Role chips are the `For Everyone ⌄` facet.** The Tasks screen had an
   unexplained persona dropdown. Here is its data source: every skill carries a
   role array (`analysts`, `founders`, `marketing`, …). The facet filters the
   catalog by role tag.
2. **`Works with` is declared, not derived.** I had assumed connector icons
   were computed from the tools a skill uses. They are an explicit field.

### The skill body format

The `Instructions` card follows a fixed four-part structure. Every one of the
117 skills is almost certainly authored to this template:

```markdown
# <Imperative title>

## When to use
<One paragraph: the triggering situation.>

## Approach
1. <Ordered steps — the actual method.>

## Heuristics
- <Bulleted rules of thumb / taste.>

## Gotchas
<One paragraph: what goes wrong, what to verify.>
```

`When to use` is what makes "drawn on automatically when it's relevant"
work — it is the retrieval/trigger description, exactly analogous to a skill
description used for automatic invocation.

### Verbatim example — `Dashboard design & building`

The only complete skill body captured. Reproduced exactly, as the authoring
reference for Flux's own skills.

> **Name:** Dashboard design & building
> **Description:** Turn a sheet or query results into a clean dashboard with the right chart types
> **Roles:** For analysts · For founders · For marketing
> **Works with:** Google Docs, Google Sheets
> **Related:** Budget vs actual variance analysis · Dataset profiling (EDA) · Product Metrics Review

```markdown
# Build a Dashboard

## When to use

Stakeholders need an at-a-glance view of KPIs from a sheet or export, with the
right charts, filters, and a clear story.

## Approach

1. Clarify the audience and the questions the dashboard must answer - design
   backward from the decisions it supports.
2. Pick the KPIs that matter and the comparisons that give them meaning
   (vs target, vs last period, vs segment).
3. Choose chart types deliberately: line for trends over time, bar for category
   comparison, single big number for a headline KPI, table for detail. Avoid
   pie charts beyond 2-3 slices.
4. Lay it out top-down: headline metrics first, supporting trends below, detail
   last. Add filters for the dimensions users will slice by.
5. Build it as a clean HTML dashboard (or in the user's BI/sheet tool), with
   consistent formatting, labeled axes, and readable number formats.
6. Add a one-line takeaway per section so the dashboard explains itself.

## Heuristics

- One chart, one question. If a chart needs a paragraph to explain, split it.
- A number without a comparison is trivia; always anchor it (target, prior period).
- Less ink, more signal - drop chartjunk, gridlines, and 3D effects.

## Gotchas

Make refresh and data source obvious, and verify the totals tie to the source
before anyone trusts it.
```

Note the quality of this content: it is genuine domain taste ("a number without
a comparison is trivia"), not restated prompt boilerplate. **The skill library
is the actual moat** — 117 of these, hand-written, is a large content
investment that no amount of model capability substitutes for.

---

## 4. Derived schema for Flux

```ts
interface Skill {
  id: string
  name: string                    // "Dashboard design & building"
  description: string             // one line, shown on the card
  icon: string
  categories: Category[]          // Sales | Recruiting | ... (tag set, not single parent)
  roles: Role[]                   // drives the "For <role>" facet
  worksWith: ConnectorId[]        // declared, not derived
  body: {
    title: string                 // "Build a Dashboard"
    whenToUse: string             // retrieval trigger — used for auto-invocation
    approach: string[]            // ordered
    heuristics: string[]          // unordered
    gotchas: string
  }
  related: SkillId[]
  source: 'builtin' | 'imported' | 'user'   // Import / New ⌄ / catalog
}
```

**[FLUX] additions to the schema:**

- `writeScope: 'readonly' | 'draft' | 'send' | 'purchase'` — the safety enum
  that is prose-only in the reference, now structural and enforced at the
  approval gate.
- `transport: Record<ConnectorId, 'api' | 'browser'>` — per the Connectors
  analysis, `worksWith` conflates authenticated APIs with browser automation.
- `provenance` on learned instruction entries.

## 5. Still unseen

- What `New ⌄` and `Import` actually accept (file format for a skill package).
- `See more →` under Suggested.
- The ⓘ tooltip next to `Skills`.
- A populated `Your skills` list, and how an adopted skill is edited.
