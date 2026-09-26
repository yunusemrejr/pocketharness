---
name: data-analysis
description: Analyze a dataset end to end — profile, clean, validate, explore, segment and report findings with uncertainty from a reproducible script. Use for CSV, spreadsheet, database or log analysis, KPI questions, trends, cohorts and "what does this data say" requests.
---

# Data analysis

The deliverable is a trustworthy answer to a question, backed by a script someone can rerun. Charts and prose come after the numbers are right.

## 1. Pin the question

Write the question as a measurable statement before touching data: population, metric (numerator and denominator), time window, grain (per user, order, day) and the decision it informs. Note what would change your answer. Ambiguous metric definitions are the most common source of wrong analysis; state the one you use.

## 2. Profile before trusting

Run code, not eyeballing: row count, columns and types, null share per column, distinct counts, min/max, duplicates on the natural key, date range and gaps, units and currencies, category spellings. Record surprises (negative quantities, future dates, test accounts, timezone shifts, a sudden jump in row count). Compare totals with a known reference when one exists (dashboard, invoice total, previous report).

## 3. Clean explicitly

Every exclusion and fix is a line of code with a comment and a count ("dropped 412 test orders, 0.8% of rows"). Never silently filter inconvenient rows. Keep raw data untouched; write cleaned outputs separately. Validate joins: expected cardinality (1:1, 1:n), rows before and after, unmatched keys. A join that multiplies rows inflates every sum after it.

## 4. Explore with the right comparisons

- Trends: plot the raw series before smoothing; check seasonality (weekday, month) and calendar effects before calling a change a trend.
- Segments: break the headline metric by the few dimensions that matter (channel, cohort, region, plan); look for Simpson's paradox when the mix shifts.
- Cohorts: group by start period and follow retention or revenue by age, not by calendar.
- Distributions: medians and percentiles for skewed data (revenue, latency); means hide whales and outliers.
- Rates: always show the denominator; small groups get intervals or are merged.

## 5. Quantify uncertainty and causality honestly

Give intervals or ranges for estimates from samples. Correlation in observational data is a lead, not a cause: name plausible confounders. For experiments or "did the change work" questions use the `statistical-experiments` skill. For models and prediction use `classical-ml-modeling`.

## 6. Report

Lead with the answer in one or two sentences, then the evidence: the key numbers (with denominators and time window), one or two charts that carry the argument (`data-viz` for honest encoding), caveats and data-quality limits, and what to do next. Keep observed facts, calculations and assumptions visibly separate. Save the analysis as a script or notebook with the data source, so every number in the report can be regenerated.

## Double-check before delivering

- Recompute the headline number a second, independent way (a different query, a pivot, a spot sum).
- Check totals reconcile across breakdowns (segments sum to the whole).
- Check units, currency, timezone and date boundaries (inclusive/exclusive end).
- Read one or two raw rows behind a surprising result.
- If a number is too good or too bad to be true, assume a bug first.
