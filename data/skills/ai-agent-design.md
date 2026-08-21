---
name: Design an agent that actually finishes the job
command: ai-agent-design
description: A design for the agent's tools, scope, and stop conditions — or a recommendation not to build one
categories: [Engineering, Docs]
roles: [engineering, founders]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

Someone wants to build an agent — something that loops, calls tools, and
decides its own next step — and needs the design settled before writing it.

## First, check whether it should be an agent at all

Agents are slower, dearer, and less predictable than the alternatives. Reach
for one only when all four of these hold:

1. **The task is genuinely open-ended.** The steps cannot be enumerated in
   advance because they depend on what is found along the way. "Turn this
   design doc into a pull request" qualifies. "Extract the invoice total"
   does not.
2. **The outcome justifies the cost.** Agents burn many model calls.
3. **The model is actually good at this task.** An agent does not make a model
   capable of something it cannot do in one shot; it just fails repeatedly.
4. **Errors are recoverable.** There is a test, a review step, or an undo.

If any is a no, use the simpler tier: a single call for extraction and
classification, a **fixed pipeline** with model calls at specific points when
the steps are known. Most things people build agents for are actually
pipelines, and the pipeline version is faster, cheaper, and debuggable.

## Design the tool surface

Tools are the agent's entire vocabulary, and the surface shapes behaviour more
than the prompt does.

**Fewer, broader tools beat many narrow ones.** Twenty granular tools mean
twenty chances to pick the wrong one. Model the tools on the *user's* verbs,
not the API's endpoints.

**Every tool needs an unambiguous name and a description that says when to use
it**, not just what it does. Ambiguity between two tools is the most common
source of wrong calls.

**Make errors instructive.** A tool that returns "error" teaches nothing. One
that returns "no element with that id — the page may have changed, call
read_page again" tells the agent how to recover. Error strings are prompts;
write them like prompts.

**Return what the next decision needs, and nothing else.** Tools that dump
large payloads fill the context and push out the reasoning that matters.

## Bound the agent

An unbounded loop is the default failure. Every agent needs, explicitly:

- **A budget** — tokens or money — enforced in code, checked *before* each
  call rather than after.
- **A step ceiling.**
- **A repetition detector.** Digest each call as `tool + arguments`; if the
  same digest recurs, the agent is stuck in a state it believes is different.
  This is the signature failure of long-running agents and it is easy to catch.
- **A consecutive-failure limit.**
- **A definition of done** the agent can actually evaluate.

## Decide what needs a human

Classify every tool by its worst possible effect: reads nothing external,
writes a draft, transmits something, spends money. Then gate on the boundary.

Two rules that matter:

- **Withhold out-of-scope tools entirely** rather than offering and refusing
  them. A model that cannot see a send tool does not argue for it.
- **Watch for effect laundering.** If "click" is unrestricted and one of the
  buttons says *Send*, the scope boundary is decorative. Inspect what a generic
  action is about to do, not just what the tool is called.

Approval prompts must state the **effect** in plain language — "Send this
email to 12 recipients" — not the tool name and arguments. The person
approving needs to know what happens, not what function runs.

## Write the system prompt last

Once tools and bounds exist, the prompt is short. It says: what the agent is
for, what to do before acting (observe first), what to do on uncertainty (say
so, don't invent), and when to stop. Long agent prompts are usually
compensating for a bad tool surface — fix the surface instead.

## Heuristics

- If you can write the steps down, write the steps down. That is a pipeline.
- The agent should observe before acting, every time.
- Cheap models for mechanical steps, capable ones for judgement. Route per
  step rather than picking one model for the whole loop.
- Log every action with its inputs. An agent you cannot replay is one you
  cannot debug.

## Gotchas

Agents fail most often not by doing something dramatic but by **quietly
believing they succeeded**. Verification has to be a real step with its own
tool call and its own evidence, not an assumption drawn from a tool returning
without an error.
