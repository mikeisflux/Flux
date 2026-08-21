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

Three skill bodies have now been captured, and they **do not share a fixed
section template**. Only one heading is constant:

| Skill | Sections after the header |
|---|---|
| Dashboard design & building | `When to use` · `Approach` · `Heuristics` · `Gotchas` |
| Chart creation from data | `When to use` · `Get the data` · `Pick the chart type` · `Build and design` · `Deliver` |
| Data warehouse context | `When to use` · `Discover` · `Interview for tribal knowledge` · `Capture and maintain` |

**`When to use` is the only required section.** It is the retrieval trigger —
what makes "drawn on automatically when it's relevant" work. Everything after
it is freeform prose authored to match how that particular job is actually
done: a charting skill gets a decision table, a context-building skill gets an
interview script, a design skill gets heuristics.

That is the right call, and worth copying deliberately. A rigid
`Approach / Heuristics / Gotchas` template would flatten every skill into the
same shape and force filler into sections that don't apply. The schema should
therefore store the body as **markdown with one required `whenToUse` field**,
not as a fixed set of typed fields.

Other structural notes from these three:

- **`Works with` is optional.** `Chart creation from data` and `Data warehouse
  context` have no connector row at all — role chips go straight to
  `Instructions`.
- **Role chip order varies** between skills (`analysts, founders, marketing`
  vs `analysts, marketing, founders`), suggesting relevance ranking rather
  than a fixed order.
- **`Related skills` looks computed, not curated.** All three list *Budget vs
  actual variance analysis* and *Dataset profiling (EDA)*, and each links the
  other two. Consistent with similarity within the Data category rather than
  hand-authored links.
- Roles observed so far: `analysts`, `founders`, `marketing`, `engineering`.

### Verbatim bodies

The three captured skill bodies, reproduced exactly, as the authoring
reference for Flux's own library.

---

#### 1. `Dashboard design & building`

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

---

#### 2. `Chart creation from data`

> **Description:** Turn query results, a table, or pasted data into a clear, honest chart
> **Roles:** For analysts · For marketing · For founders
> **Works with:** *(none)*
> **Related:** Budget vs actual variance analysis · Dataset profiling (EDA) · Dashboard design & building

```markdown
## When to use

Turning query results, a dashboard table, pasted data, or a CSV into a
publication-quality chart.

## Get the data

Read numbers off a web dashboard in the browser, paste them in, or load a CSV.
If a warehouse is connected, query it; otherwise work from what's on screen.
Clean types and nulls first.

## Pick the chart type

- Trend over time -> line. Comparison across categories -> bar (horizontal if many).
- Part-to-whole -> stacked bar or area (avoid pie unless under 6 slices).
  Distribution -> histogram or box plot.
- Correlation -> scatter. Ranking -> horizontal bar. Matrix -> heatmap.
  Flow -> Sankey. Explain the choice briefly if the user didn't specify one.

## Build and design

Use matplotlib/seaborn for static charts, plotly for interactive. Always
include a title that states the insight ("Revenue grew 23% YoY", not "Revenue
by Month"), labeled axes with units, formatted numbers ($1.2M, 45%, 2.3K), a
colorblind-safe palette, and no chart junk. Bars start at zero; sort by value
unless there's a natural order.

## Deliver

Show the chart, share the code so it can be tweaked, and suggest variations
(different type, grouping, or time range).
```

Note `Get the data` explicitly names the browser fallback — *"Read numbers off
a web dashboard in the browser... If a warehouse is connected, query it;
otherwise work from what's on screen."* The connector-as-optimization
philosophy from the Connectors tab is written directly into skill content.

---

#### 3. `Data warehouse context`

> **Description:** Build a reusable reference of your tables, metrics, terminology, and gotchas
> **Roles:** For analysts · For engineering
> **Works with:** *(none)*
> **Related:** Budget vs actual variance analysis · Dataset profiling (EDA) · Dashboard design & building

```markdown
## When to use

So future analyses understand your company's tables, terminology, metric
definitions, and quirks instead of re-deriving them every time.

## Discover

Identify the warehouse (BigQuery, Snowflake, Postgres/Redshift, Databricks)
and explore its schemas. Ask which 3-5 tables analysts query most, and pull
their columns, keys, and refresh cadence.

## Interview for tribal knowledge

- Entities: when people say "user" or "customer", what exactly do they mean,
  and which IDs link them?
- Metrics: the 2-3 most-asked metrics, their exact formulas
  (e.g. ARR = monthly_revenue x 12), and time conventions.
- Hygiene: what must ALWAYS be filtered out (test, internal, fraud, deleted)?
- Gotchas: timezones, NULL handling, historical vs current-state tables,
  confusing column names.

## Capture and maintain

Write a reference doc: entity definitions and relationships; metric formulas
with source tables and caveats; per-domain table notes with sample queries;
and standard exclusions. Update it as new domains come up.
```

**This one is architecturally interesting.** Its *output is durable context for
future runs* — a reference doc that later analyses read instead of re-deriving.
Combined with the Instructions buffer's "Polar also saves useful things it
learns here", there are two distinct memory mechanisms:

| Mechanism | Scope | Written by | Visible where |
|---|---|---|---|
| Instructions buffer | Global, every task | Agent + user | Customize › Instructions |
| Context-building skills | Domain-specific | A skill run, on demand | An artifact (doc/sheet) |

The second is arguably the better pattern — the knowledge lands in a real
document the user already knows how to read, edit, and share, rather than in
app-private state. **[FLUX]** Adopt both, and make context artifacts
first-class: a run should be able to declare "this doc is my warehouse
context" so later runs load it automatically.

---

## 3b. The `Add to my skills` modal — adoption & authoring form

Triggered by `+ Add to my skills`. Modal ~600px wide, centered, radius ~16px,
`×` close top-right.

> **Add to my skills**
> Polar will use this automatically when it's relevant. Edit anything before
> saving.

Three editable fields, then `Cancel` (text) / `Add skill` (black filled).

| Field | Control | Captured value |
|---|---|---|
| **Command** | single-line input | `dashboard-building` |
| **Description** | single-line input | `Turn a sheet or query results into a clean dashboard with the right ch…` |
| **Instructions** | scrollable textarea, resize handle | the full body as **raw markdown** |

### What this settles

**1. Skills have a slash command.** This is the field nothing else exposed.
Two samples captured:

| Skill name | Command | Derivation |
|---|---|---|
| Dashboard design & building | `dashboard-building` | abbreviated slug — drops "design &" |
| Chart creation from data | `data-create-chart` | semantic rewrite — reordered to `<domain>-<verb>-<object>` |
| Data warehouse context | `data-warehouse-context` | exact slug of the name |

All three skills are in the **Data** category, and **all three commands are
derived by a different rule**: one abbreviates, one rewrites and reorders, one
slugifies literally. There is no convention here to implement — these are
hand-authored one at a time.

Worse, the namespace is **ambiguous**. Because `data-create-chart` really is
domain-prefixed, a user seeing `data-warehouse-context` cannot tell whether it
parses as *"data-warehouse context"* (the subject) or *"data / warehouse-context"*
(category + name). Typing `/data-` and expecting to filter the Data category
gets you an inconsistent, partial list. With 117 skills plus every saved
workflow sharing one namespace, commands stop being guessable — which is the
entire reason to have commands rather than a picker.

The handle is **user-editable at adoption time**, so inconsistency is partly
self-correcting per user — but the shipped defaults set the pattern.

**[FLUX]** Pick one convention and generate to it (`<category>-<verb>-<object>`),
show the resulting command live as the user types the name, and check it for
collisions across both the skill and workflow namespaces before saving.

**1b. `Description` carries over from the catalog unchanged** — verified on all
three samples; each field is verbatim the catalog card description. So the catalog `description`
*is* the retrieval description; there is no separate trigger text besides the
`When to use` section in the body.

Combined with the Workflows empty state (*"trigger it anytime with
`/command`"*), there is a **single unified command namespace** covering both
workflows and skills:

```
/dashboard-building        → invoke an adopted skill
/<workflow-name>           → run a saved workflow
```

That is a significant piece of architecture that neither screen states on its
own. It also implies collision handling the UI never shows — nothing validates
uniqueness in the captured form.

**2. The body is stored as raw markdown, not structured fields.** The textarea
shows literal `# Build a Dashboard`, `## When to use`, `## Approach`, `1.`,
`## Heuristics`, `- `, `## Gotchas`. This confirms the corrected schema: freeform
markdown with a required `When to use` section, not typed sub-fields.

**2b. The H1 title is confirmed optional.** The Dashboard body opens with
`# Build a Dashboard`; the Chart body opens directly with `## When to use` and
has no H1 at all. `title?` in the schema is verified, not assumed.

**3. Adoption is fork-on-copy.** "Edit anything before saving" — the user gets
a mutable copy, not a reference to the catalog original. That is what "Add a
skill to make it yours" means literally. Implication: catalog updates do
**not** propagate to adopted skills, and there is no visible "upstream has
changed" affordance.

**4. Role chips, `Works with`, and `Related skills` are absent from the form.**
They appear in the read-only detail modal but are not editable here. So they
are **catalog metadata**, not user-authored fields — which means a user's own
skill (via `New ⌄`) presumably has no roles and no related links, and would
never surface under the `For <role>` facet.

**[FLUX]** Three fixes here:
- Validate command uniqueness at entry, and show the namespace the command
  lands in (skill vs workflow).
- Keep a `forkedFrom` pointer with the catalog version, so an adopted skill can
  show "the original changed" and offer a diff. Fork-and-forget silently rots.
- Let user-authored skills declare roles and connectors too, or they are
  second-class citizens in their own library.

---

## 4. Derived schema for Flux

```ts
interface Skill {
  id: string
  name: string                    // "Dashboard design & building"
  command: string                 // "dashboard-building" — kebab handle, user-editable,
                                  // shares one namespace with workflow commands
  description: string             // one line, shown on the card
  icon: string
  categories: Category[]          // Sales | Recruiting | ... (tag set, not single parent)
  roles: Role[]                   // drives the "For <role>" facet
  worksWith?: ConnectorId[]       // declared, not derived; optional
  body: {
    title?: string                // optional H1, e.g. "Build a Dashboard"
    whenToUse: string             // REQUIRED — the retrieval trigger for auto-invocation
    sections: MarkdownSection[]   // freeform; NOT a fixed template. Authored to
                                  // fit the job: decision tables, interview
                                  // scripts, heuristics, step lists.
  }
  related: SkillId[]
  source: 'builtin' | 'imported' | 'user'   // Import / New ⌄ / catalog
  forkedFrom?: { skillId: string; version: string }  // [FLUX] adoption copies; track origin
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

- What `New ⌄` and `Import` actually accept (file format for a skill package)
  — though the `Add to my skills` form implies the payload is just
  `{ command, description, instructions-markdown }`.
- Whether command collisions between skills and workflows are validated.
- `See more →` under Suggested.
- The ⓘ tooltip next to `Skills`.
- A populated `Your skills` list, and how an adopted skill is edited.
