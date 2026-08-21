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
body_status: skeleton   # frontmatter transcribed; body authored
---
## When to use

Look up a Hugging Face or arXiv paper, read it, and summarize with linked models.

## Approach

1. **Get the data.**
2. **Validate it.**
3. **Transform.**
4. **Deliver.**

## Heuristics

- State what you could not determine rather than filling the gap.
- Cite the source for every claim a reader would want to check.
- Stop and ask when the request is ambiguous in a way that changes the output.

## Gotchas

Verify the result against its source before reporting it as done.
