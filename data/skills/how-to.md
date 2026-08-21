---
name: How to do anything
command: how-to
description: A researched, current, step-by-step plan for whatever you are trying to do
categories: [Docs, Personal, Research]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

The user knows what they want to accomplish but not how — or thinks they know
and wants it checked. Deliberately broad: filing a permit, setting up a bank
account, shipping internationally, replacing a part, running a first payroll,
publishing a book.

Use this instead of answering the question directly whenever the answer is a
**process with steps, order, and consequences** rather than a fact.

## Ask first — but only what changes the plan

The instinct is to start researching. Don't. The same goal produces completely
different plans depending on a handful of variables, and researching before
knowing them wastes the effort.

Ask **at most five questions**, and only ones where a different answer
produces a different plan. The usual candidates:

- **Where.** Jurisdiction changes procedure, cost, and legality more than any
  other variable.
- **For yourself or a business**, and if a business, what structure. This
  routes into an entirely different set of rules.
- **The deadline**, if any. It determines whether the fast expensive path or
  the slow cheap one is correct.
- **The budget**, where paid and free paths both exist.
- **What has already been done.** Half-completed processes are common and
  starting from step one wastes the user's time.

Do **not** ask:

- Anything you can look up. Asking the user for information you could find is
  the fastest way to make this skill worse than a search engine.
- Anything that does not branch the plan. "What is your goal with this?" when
  they have already said it is filler.
- More than five things. Long question lists get abandoned. Ask the
  highest-leverage ones, make reasonable assumptions about the rest, and
  **state the assumptions in the plan** so they can be corrected.

If a question is genuinely blocking — a plan cannot be written without it —
ask it alone and wait. Otherwise batch them.

## Research the current way, not the remembered way

Procedures change: forms are renumbered, fees rise, portals replace paper,
requirements are added. Anything recalled from memory is a starting hypothesis,
not an answer.

- Go to the **primary source** — the agency, the company, the official
  documentation. Not a blog summarising it, and not a content farm.
- **Check the date** on everything. A 2019 guide to a government process is
  usually wrong now.
- Where a third party summarises the rule, follow it back to the rule itself.
- Note where sources **disagree**. That disagreement is usually a real
  ambiguity the user will hit, and flagging it is more useful than silently
  picking one.

## Build the plan

Structure it so the user can act without re-reading:

1. **The short answer first.** Two sentences: what this takes, roughly how
   long, roughly what it costs. Many people only need this.
2. **Prerequisites** — what must exist before step one. Documents, accounts,
   identifiers. This is where most attempts stall.
3. **Numbered steps in execution order**, each with: what to do, where to do
   it (with the actual link), what it costs, how long it takes, and what
   confirms it worked.
4. **Dependencies and waiting periods**, called out explicitly. If step 4
   cannot start until step 2 clears, and clearing takes two weeks, that
   determines the whole timeline.
5. **Deadlines**, worked backwards from any fixed date.
6. **Total cost**, itemised, separating mandatory from optional.
7. **Where it commonly goes wrong** — the rejection reasons, the missed
   requirement, the thing that has to be redone.
8. **What needs a human.** Anything requiring identity verification, a
   signature, a notary, a phone call, or a legal or medical judgement.

## Say what you are not sure about

Where the research was ambiguous, say so and say what would resolve it — a
phone number to call, an office to ask. A plan that admits a gap is far more
useful than one that papers over it, because the user finds the gap either way.

For anything with legal, medical, immigration, or tax consequence: lay out the
process and cite the source, and say plainly where a professional is the right
call. Explaining how a filing works is useful; telling someone what to file is
not this skill's job.

## Heuristics

- Steps in execution order, never grouped by theme.
- Every step gets a link to the actual page, not to a homepage.
- Cite the source and its date for anything with a fee, a deadline, or a legal
  consequence.
- If the real answer is "this is simpler than you think", say that first.
- If the honest answer is "hire someone", say that too, with what to look for.

## Gotchas

The commonest failure is a plan that is right in general and wrong for this
user's jurisdiction. When location matters and the user has not said where they
are, that is the one question worth blocking on.
