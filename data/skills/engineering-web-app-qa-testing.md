---
name: Web app QA testing
command: engineering-web-app-qa-testing
description: Recon, act, and verify flows in the browser, capturing every failure with evidence
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Vercel
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

Exercising a web application to find what is broken, before a release or against a reported problem.

## Cover the paths that matter

Start with the flows whose failure would be an incident - sign up, sign in, the core action, payment - and test them end to end as a user, not as a set of components. Then the edges around each: back button mid-flow, refresh mid-flow, double submit, session expiry, and an interrupted network.

## The inputs that break things

Empty. Whitespace only. Very long. Unicode, emoji, and right-to-left text. Leading zeros. Negative numbers where positive is expected. HTML and SQL metacharacters. A date on 29 February and one at a daylight-saving boundary. A file of the wrong type and one that is far too large.

## Beyond the happy browser

- Small viewport and large; the layout at 320px wide is where most breakage is.
- Keyboard only: can every action be reached and triggered without a mouse?
- Slow network, throttled - does the UI show a state or just nothing?
- The second tab: does the app behave when it is open twice?

## Report so it can be fixed

Every finding: exact steps, what happened, what was expected, environment and browser version, and a screenshot or recording. A bug report without reproduction steps costs the developer more time than the bug did.
