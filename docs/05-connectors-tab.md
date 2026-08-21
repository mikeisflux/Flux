# Flux — Connectors Tab

The most consequential screen captured. It settles, in the product's own
words, what a "connector" actually is — and cross-referencing it against the
catalog exposes the real architecture.

## Layout

Two-column screen (sidebar + content), no middle nav column — same shape as
Workflows, not Templates.

- **H1** `Connectors`.
- **Subtitle**, 2 lines, 15px gray — quoted in full below.
- **Section label** `AVAILABLE` — uppercase, ~11px, letterspaced, gray — left
  aligned, with **`+ Custom`** outlined button right-aligned on the same row.
- **Search** field: `Search connectors...`
- **Two-column card grid** — note this differs from Templates' three-column
  grid. Cards ~555px wide, taller left/right gutters.

### Connector card anatomy

```
┌────────────────────────────────────────────────────────┐
│  [32px    Name   [caveat badge]                    +   │
│   brand   Description of what the connector can do     │
│   icon]                                                │
└────────────────────────────────────────────────────────┘
```

- Brand icon ~32px, full color, left column.
- Title 17px, near-black.
- Description 13px gray, states **capabilities as verbs** — "Search messages,
  post updates, and find people".
- **`+` button** at the far right edge — the connect/add affordance. Ghost
  button, no border.
- Optional **caveat badge** inline after the title: amber/orange pill,
  ~11px. Three observed:
  | Connector | Badge |
  |---|---|
  | Salesforce | `API-enabled Salesforce orgs` |
  | Brex | `Requires approval` |
  | Instagram | `Business & Creator accounts only` |

  These are eligibility constraints, not status. Good pattern — the limitation
  is stated before the user tries to connect, not after it fails.

## The subtitle — the thesis statement of the product

> **Although Polar can already use any website, a connector gives it a direct
> line into an app like Gmail or Slack. For some actions, this makes Polar
> faster and more reliable than having to click through the site.**

This confirms the architecture inferred from the catalog icons:

- **The browser is the universal fallback.** Any website works with no
  integration at all.
- **A connector is an optimization**, not a capability unlock — "faster and
  more reliable", explicitly not "possible vs impossible".
- Connectors are therefore an **incremental performance layer** over a
  browser-automation substrate that already covers everything.

That is exactly the right way to build this, and it is the reason the catalog
can carry 250 templates spanning 73 services while the product ships only 37
connectors.

**`powered by Composio`** appears in the Google connector's description —
revealing they did not build the integration layer in-house. Composio is a
third-party managed-integration provider. Worth knowing: it means the
connector count can scale fast, but auth, rate limits, and data residency are
delegated to a vendor.

## The 37 available connectors

| # | Connector | Description |
|---|---|---|
| 1 | Google | Read and write Gmail, Drive, Docs, Sheets, and Calendar (powered by Composio) |
| 2 | Slack | Search messages, post updates, and find people |
| 3 | iMessage | Read and send iMessages and look up contacts on this Mac |
| 4 | Outlook | Read and send email, manage calendars, and work with Microsoft To Do |
| 5 | Apollo | Find leads, enrich data, and sequence outreach |
| 6 | Granola | Search and read AI meeting notes |
| 7 | QuickBooks | Query accounting data, run reports, and manage bills, invoices, and payments |
| 8 | Notion | Search, read, and update pages and databases |
| 9 | Linear | Triage issues and manage projects & team workflows |
| 10 | Attio | Manage CRM contacts, companies, and deals |
| 11 | Stripe | Search customers, invoices, payments, and subscriptions |
| 12 | Atlassian | Search Jira issues and Confluence pages |
| 13 | Asana | Coordinate tasks, projects, and team goals |
| 14 | Superhuman Mail | Search email, draft and send messages, and manage calendar events |
| 15 | Sentry | Debug errors, events, and performance traces |
| 16 | Cloudflare | Debug Workers, check Cloudflare edge traffic via Radar, and search docs |
| 17 | Zapier | Trigger automations across 9,000+ connected apps |
| 18 | Webflow | Manage sites, pages, and CMS collections |
| 19 | GitHub | Search code, review pull requests, and triage issues |
| 20 | HubSpot | Chat with your CRM to surface deal insights |
| 21 | Monday | Manage boards, items, and team workflows |
| 22 | Calendly | Manage event types, availability, and scheduling links |
| 23 | Airtable | Query and update tables and records |
| 24 | Grain | Search meeting recordings, notes, and transcripts |
| 25 | Salesforce `API-enabled Salesforce orgs` | Search and update accounts, contacts, leads, opportunities, tasks, reports, and campaigns |
| 26 | Brex `Requires approval` | Inspect cards, expenses, bills, banking, users, and accounting records |
| 27 | Mercury | Check balances, transactions, and team activity |
| 28 | Ramp | Track card spend, invoices, and bills |
| 29 | Cal.com | Manage scheduling links, bookings, and availability |
| 30 | Klaviyo | Build segments, send campaigns, and review email metrics |
| 31 | Coda | Search docs, read tables, and create rows |
| 32 | PostHog | Analyze product usage and manage experiments & feature flags |
| 33 | Pylon | Investigate customer issues, accounts, and conversations |
| 34 | Sanity | Query schemas and manage structured content |
| 35 | DualEntry | Search accounting records and create validated draft transactions and entities |
| 36 | Google Analytics | Inspect GA4 properties, audiences, reports, funnels, and realtime activity |
| 37 | Instagram `Business & Creator accounts only` | Publish posts, review insights and comments, and reply to existing customer DMs |

`+ Custom` implies user-defined connectors — likely MCP servers or an OpenAPI
spec. Not captured.

## Cross-reference: catalog usage vs. connector coverage

The catalog references **73 distinct services**. Only **22 of them have a
connector.** The other **51 services — 177 catalog entries — run purely on
browser automation.**

### Catalog services WITH a connector

| Service | Catalog uses |
|---|---|
| Docs | 90 |
| Sheets | 69 |
| Gmail | 41 |
| GitHub | 17 |
| HubSpot | 15 |
| Google | 13 |
| QuickBooks | 13 |
| Instagram | 12 |
| Google Drive | 11 |
| Slack | 11 |
| Google Calendar | 11 |
| Linear | 5 |
| Sentry | 4 |
| Stripe | 4 |
| Salesforce | 3 |
| Apollo | 2 |
| Atlassian | 2 |
| Notion | 2 |
| Monday.com | 1 |
| Airtable | 1 |
| Webflow | 1 |
| Cloudflare | 1 |

### Catalog services WITHOUT a connector — browser-driven only

| Service | Catalog uses |
|---|---|
| LinkedIn | 45 |
| Reddit | 13 |
| PayPal | 12 |
| TikTok | 9 |
| G2 | 8 |
| X | 6 |
| Vercel | 5 |
| Gong | 4 |
| YouTube | 4 |
| Amazon | 4 |
| Crunchbase | 4 |
| Figma | 4 |
| Greenhouse | 3 |
| TechCrunch | 3 |
| Facebook | 3 |
| Hacker News | 3 |
| Acrobat | 3 |
| Zendesk | 3 |
| Trustpilot | 2 |
| SEC EDGAR | 2 |
| Y Combinator | 2 |
| Zillow | 2 |
| OpenTable | 2 |
| Resy | 2 |
| Hugging Face | 2 |
| Intercom | 2 |
| Outreach/Salesloft | 1 |
| Ashby | 1 |
| Glassdoor | 1 |
| Product Hunt | 1 |
| Patreon | 1 |
| IRS | 1 |
| Analytics | 1 |
| Kayak | 1 |
| DoorDash | 1 |
| TodayTix | 1 |
| Chase | 1 |
| Zocdoc | 1 |
| StubHub | 1 |
| SeatGeek | 1 |
| Vivid Seats | 1 |
| Statuspage | 1 |
| SAM.gov | 1 |
| arXiv | 1 |
| FTC | 1 |
| gov registry | 1 |
| Outreach | 1 |
| Session replay | 1 |
| Ahrefs | 1 |
| Chrome DevTools | 1 |
| Chrome | 1 |



### Connectors with NO catalog template yet

- iMessage
- Outlook
- Granola
- Attio
- Asana
- Superhuman Mail
- Zapier
- Monday
- Calendly
- Grain
- Brex
- Mercury
- Ramp
- Cal.com
- Klaviyo
- Coda
- PostHog
- Pylon
- Sanity
- DualEntry
- Google Analytics

## What this cross-reference reveals

### 1. LinkedIn is the product's largest dependency and has no connector

**45 catalog entries use LinkedIn — more than any service except Google's own
apps — and there is no LinkedIn connector.** Every one of those 45 runs by
driving linkedin.com in the browser.

This is not an oversight; LinkedIn has no meaningful public write API and
aggressively blocks automation. But it means the single most-used integration
in the product is also its most fragile and the one carrying real
account-suspension risk for the user. Tasks like *"Send 15 personalized
connection requests every morning"* and *"Send connection requests to my list
on LinkedIn"* are scheduled, unattended, at-scale automation of a platform
whose ToS forbids exactly that. **Nothing in the UI warns the user.**

**[FLUX]** This needs an explicit risk disclosure on the template card and a
rate-limit governor the user can see. Getting someone's LinkedIn account
banned is a worse outcome than the task failing.

### 2. PayPal (12 uses) has no connector, but Stripe does

Both are payment processors, both appear in finance skills. Stripe got the
integration, PayPal didn't — so half the finance skills run on API and half
on screen-scraping a payments dashboard. Inconsistent reliability inside a
single skill category.

### 3. Twenty connectors have no template at all

iMessage, Outlook, Granola, Attio, Asana, Superhuman Mail, Zapier, Calendly,
Grain, Brex, Mercury, Ramp, Cal.com, Klaviyo, Coda, PostHog, Pylon, Sanity,
DualEntry, Google Analytics.

The integration surface is running **ahead** of the catalog — these are
connectors built for tasks that don't exist yet. It suggests connectors are
sourced from a vendor catalog (Composio) rather than driven by template
demand.

### 4. The two icon meanings are still not distinguished

A card showing the Gmail icon and a card showing the LinkedIn icon look
identical, but one is a rate-limited authenticated API and the other is a bot
clicking a hostile website. Their latency, failure modes, and risk are nothing
alike.

**[FLUX]** Flux marks every service on a card as either **API** or **Browser**,
and surfaces it on the task card, in the run view, and in the failure message.
The user should know before running whether the task depends on a stable
contract or on a page layout that can change tomorrow.
