---
name: Core Web Vitals Audit
command: engineering-core-web-vitals-audit
description: Profile page load performance, measure Core Web Vitals, and prioritize fixes by impact
categories: [Engineering]
roles: [engineering]
worksWith:
  - id: Chrome DevTools
    transport: browser
  - id: Chrome
    transport: browser
writeScope: readonly
body_status: authored
---

## When to use

A page is slow, or the vitals are failing, and the job is to find the specific cause rather than to make general recommendations.

## Measure field data before lab data

Lab tools tell you what could be slow; field data tells you what is slow for real users. Start with the 75th percentile of real traffic - that is what the thresholds are defined against - then reproduce in the lab.

- **LCP** under 2.5s - the largest element in the viewport.
- **INP** under 200ms - responsiveness to real interactions.
- **CLS** under 0.1 - unexpected movement.

## Diagnose each one specifically

**LCP** - identify the actual LCP element first; it is usually a hero image or a heading blocked by a font. Then which of the four phases dominates: server response, resource load delay, resource load time, or render delay.

**INP** - find the interactions with the worst latency and the long tasks blocking the main thread during them. Usually a large hydration pass, an expensive event handler, or a third-party script.

**CLS** - almost always images and iframes without dimensions, injected banners, or a font swap moving text.

## Fixes in order of effect

1. Set explicit width and height on every image and embed.
2. Preload the LCP image; do not lazy-load it.
3. `font-display: swap` with a matched fallback metric to stop the reflow.
4. Break up long tasks and defer non-critical JavaScript.
5. Audit third-party scripts - they are usually the largest single contributor and the easiest to remove.

## Gotchas

- Measure on the hardware users have, not on a developer laptop. Throttle CPU 4x.
- A single number hides route-level differences; audit per template, not per site.
- Improving the lab score without moving field data means you optimised the test.
