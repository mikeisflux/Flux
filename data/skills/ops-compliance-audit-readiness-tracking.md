---
name: Compliance & Audit Readiness Tracking
command: ops-compliance-audit-readiness-tracking
description: Track controls, evidence, and gaps for SOC 2, ISO 27001, GDPR, and more
categories: [Ops]
roles: [ops, founders]
writeScope: readonly
body_status: authored
---

## When to use

Keeping an organisation in a state where an audit is a reporting exercise rather than a project.

## Maintain the control inventory as the spine

One register: control, owner, frequency, evidence produced, where the evidence lives, and when it was last verified. Everything else hangs off this. Without an owner per control, evidence gathering becomes a search every cycle.

## Collect evidence continuously, not before the audit

The whole difference between readiness and a fire drill. Evidence gathered at the time is accurate; evidence reconstructed later is a reconstruction and looks like one. Set a cadence per control matching its frequency, and track collection as a percentage complete.

## Track gaps openly

A register of known gaps, each with an owner, a remediation plan and a date. Auditors find undocumented gaps far more damaging than documented ones with a plan - the first suggests you do not know, the second that you do.

## Gotchas

- Evidence needs to show *when* the control operated, not just that it exists. A screenshot with no timestamp proves little.
- Changes to systems and processes invalidate control descriptions. Tie the register to a change process, or it drifts silently.
- Access reviews are the most commonly failed control and the easiest to keep current. Do them on schedule.
