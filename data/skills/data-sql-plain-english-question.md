---
name: SQL from a plain-English question
command: data-sql-plain-english-question
description: Turn a plain-English data need into optimized, dialect-correct SQL
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

Someone asks a question of the data in words and expects a number back. The job is to turn the question into SQL that answers exactly it, and to say what the query does not cover.

## Pin the question down

A plain-English question is almost always underspecified, and the gap is where wrong answers come from. Before writing SQL, settle:

- **Grain.** Per user, per order, per user-day? Most disagreements about a number are a disagreement about grain.
- **Window.** Calendar month or trailing 30 days? Which timezone? Warehouse timestamps are usually UTC and the business usually is not.
- **Population.** Are test accounts, internal users, refunded orders and soft-deleted rows in or out?

If any of the three is genuinely ambiguous, state the assumption in the output rather than picking silently.

## Write it to be checked, not to be clever

- One CTE per idea, named for the idea. A reader should be able to run any CTE alone and understand its output.
- Filter early in a CTE rather than late in a `WHERE` on the join result.
- Prefer explicit `JOIN` over correlated subqueries; a fan-out is visible in a join and invisible in a subquery.
- No `SELECT *` in anything that survives past the scratch pad.

## Verify before reporting

Run these every time, because they catch most of what goes wrong:

1. **Row count before and after each join.** A join that multiplies rows is the single most common cause of an inflated total.
2. **A known value.** Pick one entity you can check by hand and confirm the query agrees.
3. **NULL check on the join keys.** `NULL` keys silently drop rows in an inner join.
4. **Sanity on the total.** If revenue for the month is 40x last month, the query is wrong before the business is.

## Gotchas

- `COUNT(column)` skips NULLs, `COUNT(*)` does not. This is the quietest bug in SQL.
- Averaging an average gives the wrong answer unless every group is the same size.
- `BETWEEN` on timestamps includes the endpoint, so a day range written with `BETWEEN` double-counts midnight.
- Report the number **with** the query that produced it. A figure nobody can re-derive is not an answer.
