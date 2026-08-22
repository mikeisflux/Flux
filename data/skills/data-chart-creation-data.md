---
name: Chart creation from data
command: data-chart-creation-data
description: Turn query results, a table, or pasted data into a clear, honest chart
categories: [Data]
roles: [analysts, engineering]
writeScope: readonly
body_status: authored
---

## When to use

A sheet or a query result needs to become a finished chart someone else will read without you present.

## Get the data into chart shape

Most charting failures are data shape failures.

1. **Long, not wide** - one row per (category, period, value). Most tools want this and most spreadsheets are the other way round.
2. **Complete the grid** - missing periods must be present with a zero or an explicit null, or the line lies about the gap.
3. **One unit per series.** Never mix counts and rates on the same axis.
4. **Sort deliberately** - by value for rankings, by time for series.

## Choose the chart

Match the chart to the comparison, not the data type: time is a line, ranking is a sorted horizontal bar, relationship is a scatter, distribution is a histogram. If two ideas are in play, make two charts.

## Design so it survives being forwarded

- Title states the finding, in words.
- Axis labels carry units; the axis is not the place for a cryptic column name.
- Label series directly rather than using a legend.
- Annotate the one point that matters - the launch, the outage, the price change.
- Include the date range and the source query or sheet somewhere on the chart.

## Gotchas

- A chart of a rate needs its denominator visible somewhere, or a 100% figure on n=3 gets quoted.
- Colour must survive greyscale printing and colour blindness; do not carry meaning in red-versus-green alone.
- Check the chart at the size it will actually be seen. Most charts are read in a slide or a message, not full screen.
