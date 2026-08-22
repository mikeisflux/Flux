---
name: UX copywriting
command: docs-ux-copywriting
description: Microcopy, error messages, empty states, and CTAs that help users act
categories: [Docs]
roles: [everyone]
writeScope: readonly
body_status: authored
---

## When to use

Writing the words inside a product - buttons, labels, empty states, errors - where every word is load-bearing.

## Write the way the user thinks about the task

Use the words the user would use, not the ones the system uses internally. If the codebase calls it a `workspace_membership` and the user calls it "who's on my team", the interface says team.

Buttons say what happens: **Save**, **Send invite**, **Delete forever**. Never **OK**, never **Submit**. A person should be able to press a button correctly without reading the sentence above it.

## Errors have three jobs

1. Say what happened, without blame and without a code as the headline.
2. Say why, if you know.
3. Say what to do next, as an action they can take.

"Something went wrong" does none of the three. "We couldn't save your changes - your session expired. Sign in again and we'll keep your draft." does all three.

## Empty states are the best teaching moment

The first time someone sees a screen it is empty. That is the one moment they will read an explanation. Say what goes here, why it is useful, and give them the single action that fills it.

## Gotchas

- Sentence case everywhere except proper nouns. Title Case On Buttons Reads As Shouting.
- Cut every "please", "simply", "just", and "easily". They add length and, when the thing is not easy, insult.
- Write the confirm dialog's button to say the verb - **Delete project**, not **Yes** - because that is what gets read at speed.
- Test the longest plausible string. Copy that only fits in English is a bug.
