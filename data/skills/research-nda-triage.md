---
name: NDA triage
command: research-nda-triage
description: Screen an NDA against standard carveouts and classify it green, yellow, or red
categories: [Research]
roles: [analysts, founders]
worksWith:
  - id: Docs
    transport: api
  - id: Google Drive
    transport: api
  - id: Acrobat
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

An NDA arrives and needs a quick decision: sign, redline, or escalate.

## Check the six things that decide it

1. **Mutual or one-way?** A one-way NDA where you will also disclose is the most common problem.
2. **Definition of confidential information** - is it bounded, and are the standard exclusions present (already known, independently developed, publicly available, required by law)?
3. **Term** - of the agreement and, separately, of the confidentiality obligation. Perpetual obligations on ordinary commercial information are a redline.
4. **Permitted disclosure** - can you tell your own advisers, affiliates and contractors?
5. **Residuals** - a residuals clause substantially weakens the protection; know which side of it you are on.
6. **Governing law and jurisdiction** - a far jurisdiction makes enforcement theoretical.

## Triage to one of three outcomes

- **Standard, sign** - mutual, bounded, standard exclusions, sensible term.
- **Redline** - one or two specific clauses, with the replacement wording ready.
- **Escalate** - non-compete or non-solicit language, IP assignment, anything unusual in the definitions. These are not NDA clauses and their presence in an NDA is the finding.

## Gotchas

- An NDA containing IP assignment or exclusivity is not an NDA. Always escalate.
- Check whether it binds affiliates you cannot actually bind.
- This is triage, not legal advice; anything not clearly standard goes to a lawyer.
