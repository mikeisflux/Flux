---
name: AI paper reading & digest
command: data-ai-paper-reading-digest
description: Look up a Hugging Face or arXiv paper, read it, and summarize with linked models
categories: [Data]
roles: [analysts, engineering]
worksWith:
  - id: Hugging Face
    transport: browser
  - id: arXiv
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

A stack of papers or preprints to get through, and the output has to be what changes for us - not a summary.

## Triage first

Do not read linearly. For each paper, in five minutes: abstract, figures, and the conclusion. Then decide read-in-full, skim, or discard, and write one line saying why. Most papers are a discard and saying so quickly is the whole value of triage.

## Read the ones that survive properly

For each, extract:

- **The claim**, in one sentence, in plain language.
- **The evidence** - what was actually measured, on what, at what scale.
- **The baseline** it beats, and whether that baseline was tuned as hard as the method.
- **The cost** - compute, data, latency. A method that wins at 10x the cost is a different claim.
- **What would have to be true** for it to apply to us.

## Be sceptical in specific ways

- Benchmark results without variance across seeds are one run.
- A comparison against an undertuned baseline is the most common way to win.
- Test-set contamination is endemic in anything trained on web data.
- Ablations missing for the component the paper says matters most is a tell.

## Digest format

One paragraph per paper: claim, evidence quality, and the "so what for us" - which should usually be "nothing yet". Then a short list of the two or three that would change a decision, with what we would have to try to find out.
