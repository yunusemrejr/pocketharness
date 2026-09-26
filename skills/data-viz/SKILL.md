---
name: data-viz
description: 'Design honest charts and dashboards: choose encodings, scales, labels and layout for the question. Use for readable comparisons, trends and analytical communication.'
---


# Data Visualization

## First: what question, what chart

The chart type is a consequence of the question — pick the question first:

| Question | Chart | Not |
|---|---|---|
| Compare magnitudes (A vs B…) | **horizontal bar** (sorted) | pie, line |
| Trend over time | **line** (one per series) | stacked bar for > 3 series, line for < 3 points |
| Part of whole (few parts) | 100% stacked bar or pie (≤ 4 slices) | dual-axis |
| Composition over time | stacked area / stacked bar | (stacked bar when series are few) |
| Distribution | histogram / box / violin | bar (per bin) |
| Relationship between two variables | scatter | line (spurious continuity) |
| Rank / pareto | sorted bar + cumulative line (pareto) | table with arrows |
| Single KPI with context | number + delta + sparkline + target band | gauge, traffic light |
| Flow / movement | sankey (if truly about flow) | (sankey is for showing where volume goes, not "pretty") |

- One chart, one job. "Overview dashboard chart" = a failure state; split it.
- If a table is clearer (< 10 rows, reader needs exact values), a table is the chart. Style it (right-align numbers, tabular-nums).

## Honesty (non-negotiable)

- **Number axes start at 0** unless truncating genuinely helps and you mark the break (and never truncate bars — bars are magnitude, line charts may start non-zero with a clear axis).
- No dual-axis for two different units (it lies by construction: rescale the right axis and the "correlation" disappears). Two lines, one axis, normalized if units truly differ — and label the normalization.
- Bar lengths = values; 3D, tilted, rotated charts distort — never.
- Don't sort a time series by value ("weekly revenue, sorted" = a bar in wolf's clothing).
- Gaps in time series: **break the line or mark the gap** — don't connect across missing data (implies values you don't have). Holiday/zero-weeks: annotate or handle explicitly.
- Aggregation label: "total of 14 countries" / "weekly total" — the unit of the mark must be stated.

## Color & marks

- Category color: ≤ 6 hues, colorblind-safe (Okabe–Ito palette is the safe default; avoid red/green pairs as the *only* encoding — pair with shape or direct label).
- Sequential (ordered magnitude): single-hue light→dark or perceptual (viridis family) — 90% of time-series heatmaps want this, not a rainbow.
- Diverging (± from a baseline): light–dark around a neutral, symmetric scale, anchor the midpoint explicitly.
- Gradients on a line = false precision; one solid hue per series.
- Direct labels beat legends when marks < 5: label the line/bar itself. Legends for > 5 or repeated-encoding.
- Background: white/neutral; gridlines thin, horizontal only for time series; drop the frame.
- **Data-ink ratio**: remove chart junk (3D, heavy grid, borders, decorative icons-in-bars). If a pixel doesn't carry data, it's either cut or moved to a label.

## Implementation rules

- **Mark budget:** ≤ ~15–20 bars per chart (aggregate beyond); ≤ ~10 lines per line chart (otherwise → facet); scatter density > ~2k points → 2D density / hexbin / sample with a stated sampling ratio.
- **Tooltips:** value + unit + context (compare to target / previous period) — not just the raw number. Touch targets on mobile (bigger hit areas, tap-to-pin).
- **Interaction as drill-down:** click bar → filter the whole dashboard (shared filter state), not "click shows a tooltip with a link."
- **Axes:** consistent across a comparative set (same y-range when panels are meant to be compared); auto-range per panel is OK only when panels are independent.
- **Numbers in the UI:** thousands separators, consistent decimal places (one, per column), no "1,234.567" floats in money (use currency formatting, 2 dp), dates localized (UTC-labeled for dashboards).
- **Animations:** enter ≈ 200–400 ms, update = crossfade/interpolate values (d3 transition), **no animation on initial load of a dashboard** (8 charts animating = noise), no gratuitous rotation/bounce.

## Dashboards

- **Hierarchy:** one "hero" answer per page (what the user opens it for) top-left; secondary rows; drill-aways collapsed. A dashboard where no panel is bigger is a data dump.
- One time dimension per view (mixing daily/weekly/mixing months and days = unreadable).
- ≤ 6–8 panels; shared filters (date range, segment) at top, applied everywhere, state in the URL (shareable link = screenshot).
- Every panel states its window: "last 28 days, daily." Stale data states: "updated 4 min ago" / "stale" — a dashboard lying about freshness is worse than a broken one.
- Empty & error states per panel (no data ≠ zero data — don't draw a zero line for "no data").
- Comparison framing: every KPI has its reference in view (target line, previous period, cohort) — a number alone is decoration.
- Mobile: dashboards are desktop-first; mobile = hero + top-3 KPIs + "open on desktop," not a squished grid.

## Tools

| Tool | Use it when |
|---|---|
| **D3** | custom shapes/interactions, you own the SVG; steep but the ceiling |
| **Observable Plot** | fast correct standard charts, in notebooks/docs; ~10 lines |
| **Vega-Lite** | declarative, repeated structures (facet grids, multi-series), one grammar to learn |
| **ECharts** | dense dashboards, map + big numbers (10k+ points), option-driven |
| **Recharts / Chart.js / Nivo** | React/common-web: standard charts, fast, themed to your design system |
| **Table + sparkline (no lib)** | < 10 rows: it's usually the right answer |

- Wrap your chosen lib in **2–5 typed, theme-aware components** (Line, Bars, Spark, HeatTable) — every chart in the product goes through them (design-system linkage: consistent axes/color/tooltips); direct library usage elsewhere = drift.
- Color/typography pulled from design tokens (see `design-systems`, `colors`), not hard-coded per chart.

## Accessibility

- Every chart: `aria-label` summarizing the take ("revenue up 12% QoQ"), plus a **data table fallback** (visually-hidden) for the same marks — screen readers can't read a line.
- Non-color encoding for any distinction (shape/pattern/label) — colorblind users are the floor, not an add-on.
- Keyboard-navigable marks for interactive charts (focusable, announce value); focus order = reading order.

## Anti-slop tells

Rainbow heatmaps · gauges and traffic lights for KPIs (a number + delta is better) · 3D pies & donuts · auto-sorted "trend" charts · dual-axis mystery correlations · more than one insight per chart · panel with no title and no unit · "last 30 days" that silently excludes the current partial day · animated sparkline bouncing on a 1-value update.

## Detailed coverage

Data visualization & dashboards — choosing chart type by question, honest scales, color encodings (colorblind-safe), direct labeling, mark budgets, time-series pitfalls, dashboards that have hierarchy, and tool guidance (D3, Observable Plot, Vega-Lite, ECharts, Recharts). Use when building any chart, dashboard, or "make this number readable" task.
