---
name: Incident response runbook
command: engineering-incident-response-runbook
description: Drive incident severity, status comms, and a blameless postmortem to closure
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Atlassian
    transport: api
  - id: Linear
    transport: api
  - id: GitHub
    transport: api
writeScope: readonly
body_status: authored
---

## When to use

Writing the document someone will read at 3am, half awake, under pressure, having never seen the system.

## Write for the state the reader is in

The reader is stressed, tired, and not the author. That dictates everything:

- Numbered steps, one action each. No paragraphs.
- Exact commands, copy-pasteable, with the placeholders marked clearly.
- Expected output after each step, so the reader knows whether it worked.
- The decision points explicit: "if X, go to step 7; otherwise continue".

## The structure

1. **How to tell this is the right runbook** - the symptom, the alert name, the dashboard.
2. **Immediate mitigation** first, diagnosis second. Stop the bleeding before understanding it.
3. **Diagnosis** - what to check, in the order that eliminates the most.
4. **Resolution** for each identified cause.
5. **Verification** - how to confirm it is actually fixed, not just quiet.
6. **Escalation** - who to wake, with what information, and after how long.

## Test it

A runbook that has never been executed is fiction. Walk it in a game day with someone who did not write it, and fix every step where they hesitated. Hesitation is the defect.

## Gotchas

- Link to dashboards and consoles directly. Nobody finds a dashboard by description at 3am.
- Include the rollback, and how to tell whether rollback is safe with respect to data.
- Date it and name an owner. An out-of-date runbook is more dangerous than none, because it is followed.
