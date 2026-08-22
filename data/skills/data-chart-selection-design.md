---
name: Chart selection & design
command: data-chart-selection-design
description: Chart-type selection, design principles, and accessibility for honest visuals
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

There is a number or a table and it needs to become a chart that makes the point without misleading anyone.

## Pick from the comparison, not from the data type

What is being compared decides the chart:

| Comparison | Chart |
|---|---|
| Change over time | Line; bars only if the periods are few and discrete |
| Parts of a whole, one moment | Stacked bar or a table. Not a pie beyond three slices |
| Ranking | Horizontal bar, sorted by value |
| Two variables' relationship | Scatter, with the trend stated in words |
| Distribution | Histogram or box plot |
| Two dimensions at once | Small multiples, not a dual axis |

## Rules that hold every time

- Bar charts start at zero. Line charts need not, but say so on the axis.
- Sort bars by value unless the category has a natural order.
- Label the thing being compared directly on the chart rather than in a legend the reader has to look up.
- One idea per chart. Two ideas is two charts.
- The title states the finding, not the contents: "Churn doubled in EMEA" beats "Churn by region".

## Never

- Dual y-axes. They let the author choose the scales that make any two series look correlated.
- 3D anything.
- A pie chart with more than three slices.
- A truncated y-axis on a bar chart.
