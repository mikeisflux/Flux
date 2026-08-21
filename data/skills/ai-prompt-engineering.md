---
name: Write a prompt that actually works
command: ai-prompt-engineering
description: A prompt rewritten to be specific, testable, and free of scaffolding that hurts modern models
categories: [Docs, Engineering]
roles: [everyone]
worksWith:
  - id: Docs
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

A prompt is producing the wrong thing, inconsistent things, or nearly-right
things that need editing every time. Also when writing a prompt that will run
repeatedly — inside a workflow, a skill, or an app — where consistency matters
more than any single output.

## Diagnose before rewriting

Most bad prompts fail for one of four reasons. Identify which before changing
anything, because the fixes are different:

1. **Underspecified output.** The model does not know what shape you want. The
   symptom is output that is reasonable but never the format you needed.
2. **Missing context.** The model does not know something you do. The symptom
   is confident, plausible, wrong.
3. **Buried instruction.** The real ask is at the end of three paragraphs of
   preamble. The symptom is the model answering a different question.
4. **Conflicting instructions.** "Be thorough but brief." The symptom is
   output that swings between extremes across runs.

## Rewrite

**Say the task first.** One sentence, at the top, in the imperative. Context
and constraints come after. A prompt whose real request appears in the last
line is the most common fixable failure.

**Describe the output concretely.** Not "write a summary" but "write three
bullets, each under twenty words, covering what changed, why, and what it
means for the reader." Vagueness in, variance out.

**Give an example when the format matters.** One good example does more than a
paragraph describing the format. Two examples do more than one, especially
when they differ in a way that shows the boundary.

**State what to do with uncertainty.** Every prompt that touches facts should
say what to do when the answer is not available: say so, ask, or flag it. This
single line eliminates most confabulation, and almost nobody includes it.

**Cut the conflicting constraint.** If two instructions pull against each
other, decide which one you actually want. The model cannot resolve a
contradiction you have not resolved.

## What to stop doing

Prompting habits accumulated on older models actively hurt on current ones:

- **Roleplay preambles.** "You are a world-class expert in..." adds nothing to
  a modern model and consumes attention. State the task instead.
- **Threats and bribes.** "This is very important to my career", "I'll tip
  $200". These were folklore, and they are noise.
- **Elaborate step-by-step scaffolding** for tasks the model already decomposes
  well. Over-prescribing the method on a capable model reduces output quality —
  it constrains an approach that would have been better chosen freely. Specify
  the *outcome* tightly and the *method* loosely.
- **"Think step by step"** as a reflexive addition. Current models reason
  without being told to, and the instruction mostly adds preamble.
- **Repeating the instruction three times** in different words. This makes
  conflicts more likely, not compliance.

The general rule: **be specific about what you want, permissive about how it
gets there.** Older advice inverted this because older models needed the
scaffolding. They no longer do.

## Test it properly

A prompt that worked once has not been tested.

1. Run it **five times on the same input.** Variance across runs is the real
   measure — a prompt that produces five different shapes is unusable in a
   workflow regardless of how good the best one is.
2. Run it on **the hard cases**: empty input, ambiguous input, input that
   should produce "I don't know", input at the length limit.
3. Have the failing outputs in front of you when you edit. Rewriting from
   imagination reintroduces the original problem.

## Heuristics

- If you cannot state what a correct output looks like, the prompt is not the
  problem yet.
- Length is not quality. The best prompts are usually shorter than the ones
  they replaced.
- Every instruction should change the output. Delete any that would not.
- Fix one thing at a time, or you will not know what worked.

## Gotchas

A prompt tuned hard against one model often degrades on the next one, because
much of the tuning was compensating for that model's specific weaknesses. When
switching models, strip the workarounds back out and re-test before adding
anything — the new model usually needs less, not more.
