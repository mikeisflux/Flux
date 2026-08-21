# Flux — Workflows Tab & Connector Inventory

## Part 1 — The Workflows screen

Captured in its **empty state** (`No workflows yet`), so the populated list
view is still unseen.

### Layout

Note: unlike Templates, Workflows is a **two-column** screen — sidebar +
content. There is no middle nav column.

```
│ SIDEBAR │ CONTENT                                    [New workflow ⌄] │
│ 305px   │  (centered empty state, then example grid)                  │
```

- **H1** `Workflows`, top-left of content.
- **Primary action**, top-right: `New workflow ⌄` — black pill, white text,
  ~44px tall, radius 10px, with a **chevron**. The chevron means this is a
  dropdown, not a single action — there is more than one way to create a
  workflow (likely: from scratch / from a task / from a template).
  This is the **only filled black button in any captured screen** — Templates
  has no primary action at all.

### Empty state (centered, ~700px column)

- Icon: two overlapping rounded-square nodes, ~32px, thin stroke, gray.
- Heading `No workflows yet` — ~22px semibold.
- Body, 15px gray, two lines:

  > A workflow is a task you save once and reuse — run it on a schedule, or
  > trigger it anytime with `/command`.

**This sentence is the most information-dense copy in the product.** It
defines workflow = *saved, reusable task*, and reveals a second, previously
invisible invocation path:

`/command` renders as an **inline code chip** — light blue fill (~`#e8f2fd`),
rounded ~4px, monospace. It is the only code-styled text and the only blue
pixel in the entire captured UI. Nothing in the Templates screens hints that
slash-command invocation exists.

So workflows have **three** triggers, only one of which the Templates screen
advertised:
1. **Schedule** — cron, shown as a prose chip.
2. **Slash command** — `/command`, typed on demand.
3. **Manual run** — implied.

### `Start from an example`

Section header, then a 3 × 2 grid of six cards — identical card component to
the Templates task card (title, outcome, connector icons, schedule chip).

| Title | Outcome | Connectors | Schedule |
|---|---|---|---|
| Turn last month's email receipts into an expense report | A categorized monthly expense summary built from receipts sitting in your inbox | Google, Gmail | Monthly on the 1st at 7am |
| Log prices for products I'm watching every day | A price history sheet for your watchlist with drops called out as they happen | Docs, Sheets | Daily at 7:30am |
| Summarize competitor product updates every week | A Monday brief on every feature competitors shipped in the last week | — | Mondays at 9am |
| Read a company's strategy from its job postings monthly | A monthly read on team growth, new functions, and strategy shifts from job posts | LinkedIn | First of the month at 9am |
| Queue my usual lunch order every workday | Your usual order carted on time and held at checkout for your OK | DoorDash | Weekdays 11:30 AM |
| Watch a booked-out restaurant for a table | An instant alert with the exact slot and booking link the moment a table opens | OpenTable, Resy | Every 3 hours |

**Every one of the six is an existing Tasks-catalog template, and every one
carries a schedule.** Drawn from Data (2), Research (1), Ops (1), Personal (2).

Footer: `Browse all examples` — outlined button, book icon, centered. Almost
certainly deep-links to Templates › Tasks with the `⟳ Scheduled` facet applied,
which explains why that facet exists on the Tasks screen.

### Confirmed: the task → workflow promotion model

The Tasks subtitle said *"Use a task as a starting point, or save it as a
workflow that runs on a schedule."* This screen confirms it end to end:

```
Template (catalog, 250)  →  Task (a run)  →  Workflow (saved + scheduled + /command)
```

A workflow is not a separate authoring surface — it is a **task with a name, a
schedule, and a command binding**. That is why ~40% of templates ship a
schedule: they are pre-promoted workflows waiting to be adopted.

### Still unseen on this screen

The populated list: how a saved workflow renders, run history, last-run status,
enable/disable, editing the schedule, and what the `New workflow ⌄` dropdown
contains.

**[FLUX]** The empty state has no notion of a workflow that is *failing* — no
health, no last-run status, no error surface. For scheduled automation that
runs unattended, that is the single most important thing to show. Flux's
workflow row leads with last-run outcome and a failure streak badge.

---

## Part 2 — Connector inventory

Polar never lists its connectors anywhere in the captured UI — the Connectors
tab was not captured, and the catalog only shows brand icons on cards. This
inventory is **derived** by decoding every icon across all 250 tasks and 117
skills.

**73 distinct third-party services.** Usage counts are how many catalog entries
display that icon (an entry with a `+1` overflow chip hides one more, so true
counts are slightly higher).

| Connector | Uses | Appears in |
|---|---|---|
| Docs | 90 | skills + tasks |
| Sheets | 69 | skills + tasks |
| LinkedIn | 45 | skills + tasks |
| Gmail | 41 | skills + tasks |
| GitHub | 17 | skills + tasks |
| HubSpot | 15 | skills + tasks |
| Reddit | 13 | tasks |
| Google | 13 | skills + tasks |
| QuickBooks | 13 | skills + tasks |
| Instagram | 12 | tasks |
| PayPal | 12 | skills + tasks |
| Google Drive | 11 | skills + tasks |
| Slack | 11 | skills + tasks |
| Google Calendar | 11 | skills + tasks |
| TikTok | 9 | skills + tasks |
| G2 | 8 | skills + tasks |
| X | 6 | tasks |
| Vercel | 5 | skills + tasks |
| Linear | 5 | skills + tasks |
| Gong | 4 | tasks |
| YouTube | 4 | tasks |
| Amazon | 4 | tasks |
| Crunchbase | 4 | skills + tasks |
| Sentry | 4 | skills + tasks |
| Stripe | 4 | skills |
| Figma | 4 | skills |
| Salesforce | 3 | skills + tasks |
| Greenhouse | 3 | tasks |
| TechCrunch | 3 | tasks |
| Facebook | 3 | skills + tasks |
| Hacker News | 3 | tasks |
| Acrobat | 3 | skills |
| Zendesk | 3 | skills |
| Apollo | 2 | tasks |
| Trustpilot | 2 | tasks |
| SEC EDGAR | 2 | tasks |
| Y Combinator | 2 | tasks |
| Zillow | 2 | tasks |
| OpenTable | 2 | tasks |
| Resy | 2 | tasks |
| Atlassian | 2 | skills + tasks |
| Hugging Face | 2 | skills |
| Intercom | 2 | skills |
| Notion | 2 | skills |
| Outreach/Salesloft | 1 | tasks |
| Ashby | 1 | tasks |
| Glassdoor | 1 | tasks |
| Product Hunt | 1 | tasks |
| Monday.com | 1 | tasks |
| Airtable | 1 | tasks |
| Webflow | 1 | tasks |
| Patreon | 1 | tasks |
| IRS | 1 | tasks |
| Analytics | 1 | tasks |
| Kayak | 1 | tasks |
| DoorDash | 1 | tasks |
| TodayTix | 1 | tasks |
| Chase | 1 | tasks |
| Zocdoc | 1 | tasks |
| StubHub | 1 | tasks |
| SeatGeek | 1 | tasks |
| Vivid Seats | 1 | tasks |
| Statuspage | 1 | tasks |
| SAM.gov | 1 | tasks |
| arXiv | 1 | skills |
| FTC | 1 | skills |
| gov registry | 1 | skills |
| Outreach | 1 | skills |
| Session replay | 1 | skills |
| Ahrefs | 1 | skills |
| Chrome DevTools | 1 | skills |
| Chrome | 1 | skills |
| Cloudflare | 1 | skills |



### What the distribution says

1. **Google Workspace is the substrate, not a connector.** Docs (90), Sheets
   (69), Gmail (41), Drive (11), Calendar (11) — Google surfaces account for
   roughly half of all connector usage. The product's default output format is
   a Google Doc or Sheet. Any Flux clone that ships without deep Docs/Sheets
   write support is missing the floor of the product.
2. **LinkedIn (45) is the single biggest non-Google dependency**, and it is
   the most aggressively anti-automation site on the list. It carries real
   account-ban risk that the UI never mentions.
3. **The long tail is very long and very shallow** — 40 of 73 connectors are
   used exactly once (DoorDash, TodayTix, Zocdoc, Chase, SAM.gov, Vivid Seats).
   These almost certainly are not real OAuth integrations; they are **the agent
   driving the live site in the browser**. That is the whole thesis of an agentic
   browser: the browser *is* the universal connector, so a one-off site costs
   one template, not one integration.
4. Therefore the connector icons mean two different things that the UI does not
   distinguish: *authenticated API integration* (Gmail, HubSpot, QuickBooks) vs.
   *a site the agent logs into and clicks*. **[FLUX]** These must be visually
   distinguished — their failure modes, latency, and risk profiles are nothing
   alike.

### Grouped by kind

| Kind | Services |
|---|---|
| Google | Docs, Sheets, Gmail, Drive, Calendar, Google Search |
| CRM / sales | HubSpot, Salesforce, Apollo, Gong, Outreach/Salesloft |
| Social | LinkedIn, X, Instagram, TikTok, YouTube, Facebook, Reddit, Hacker News |
| Dev | GitHub, Linear, Vercel, Sentry, Cloudflare, Atlassian, Chrome DevTools |
| Finance | QuickBooks, Stripe, PayPal, Chase, IRS |
| Support | Zendesk, Intercom, Statuspage |
| Hiring | Greenhouse, Ashby, Glassdoor |
| Research | Crunchbase, G2, Trustpilot, SEC EDGAR, TechCrunch, Y Combinator, Product Hunt, arXiv, Hugging Face, Ahrefs, FTC, SAM.gov |
| Docs / design | Notion, Figma, Acrobat, Airtable, Webflow, Slack |
| Consumer | Amazon, Zillow, Kayak, OpenTable, Resy, DoorDash, TodayTix, Zocdoc, StubHub, SeatGeek, Vivid Seats, Patreon, Monday.com |
