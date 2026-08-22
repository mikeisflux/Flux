---
name: Structured debugging
command: engineering-structured-debugging
description: Reproduce, isolate, diagnose root cause, and fix – systematically
categories: [Engineering]
roles: [engineering]
writeScope: readonly
body_status: authored
---

## When to use

A bug is reported and the cause is not obvious. The job is to find it by narrowing rather than by guessing.

## Reproduce before anything else

A bug you cannot reproduce is a bug you cannot verify fixed. Get to a reliable reproduction and write down the exact steps, the environment, and the frequency. If it reproduces one time in ten, say ten and script it - intermittent is a property to measure, not a reason to stop.

## Narrow by bisection

The only reliably fast technique. Halve the search space each step:

- **In time** - `git bisect` between a known-good and known-bad revision.
- **In the stack** - is the wrong value already wrong at the API boundary, or does it go wrong in the client?
- **In the data** - does it happen with one record or all of them? Find the smallest input that triggers it.
- **In configuration** - default config against the failing one, changed one flag at a time.

Each bisection step should eliminate half the remaining possibilities. If it does not, the split was the wrong one.

## State a hypothesis before each test

Write down what you expect the next test to show. When it shows something else, that surprise is the most valuable information in the whole exercise - it means a belief about the system is wrong, and that belief is usually the bug.

## Prove the fix

- Show the failing case failing before the change and passing after.
- Explain why it failed, in one sentence. "It works now" is not a diagnosis and usually means the bug moved.
- Add a regression test at the level the bug actually lived at.
- Look for the same mistake elsewhere in the codebase. Bugs of a kind travel in groups.

## Gotchas

- Heisenbugs that vanish under a debugger are usually timing or memory ordering.
- Works-on-my-machine is a difference in environment, and the difference is the bug.
- Do not fix two things at once. If you do, you learn nothing from either.
