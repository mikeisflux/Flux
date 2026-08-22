---
name: SEO audit
command: engineering-seo-audit
description: Check titles, headings, metadata, speed, and content gaps, then prioritize fixes
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Ahrefs
    transport: browser
  - id: Google
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Diagnosing why a site is not being found, in a way that produces a ranked list of fixes rather than a list of observations.

## Check in order of severity

1. **Indexability** - `robots.txt`, `noindex` tags, canonical tags pointing somewhere unintended, and whether the pages that matter are actually in the index. A page that cannot be indexed cannot rank, so nothing else matters until this is clean.
2. **Crawlability** - internal linking, orphan pages, redirect chains, and the crawl budget being spent on parameter URLs.
3. **Content rendering** - if the content only exists after JavaScript runs, check what a crawler actually sees.
4. **Duplication** - the same content on several URLs, with no canonical, splitting signals.
5. **Performance** - Core Web Vitals, which are a tiebreaker rather than a lever.

## Match pages to intent

Ranking failures are as often a content problem as a technical one. For the target queries, look at what actually ranks and what format it is in - a comparison table, a how-to, a tool. A page in the wrong format for the intent will not rank however clean the markup is.

## Report as ranked actions

Each finding: the specific URLs affected, the estimated impact, the effort, and the exact change. "Improve internal linking" is not an action; "add links to these 12 orphan pages from the category index" is.

## Gotchas

- Verify against the search engine's own tooling rather than a third-party crawler alone; they disagree, and only one of them decides.
- A site migration is the highest-risk SEO event there is - check redirects one to one, not in aggregate.
- Do not recommend anything that manipulates rather than improves. It works until it does not, and then it works negatively.
