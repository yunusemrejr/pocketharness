---
name: financial-modeling
description: Build and check business financial models — unit economics (CAC, LTV, payback, margins), revenue and cost forecasts, budgets, runway, three-statement links, DCF valuation and scenario/sensitivity analysis — with explicit assumptions and verifiable formulas. Use for startup or SMB finance, pricing math, forecasts and valuation work.
---

# Financial modeling

A model is a set of labelled assumptions and the arithmetic that follows from them. Its value is that anyone can see which inputs drive the answer and change them.

## Structure

- **Inputs** in one place, each with unit, period, source and date ("monthly churn 3.2%, Stripe export 2026-08"). Separate observed history from assumptions.
- **Calculations** reference inputs only; no hard-coded numbers inside formulas.
- **Outputs** (runway, valuation, payback) are derived, never typed.
- Consistent periods (monthly vs annual), currency and sign convention (costs negative or positive, never mixed). Real vs nominal stated.

Spreadsheet deliverables: live formulas, not pasted values (`spreadsheet-authoring` skill). Script deliverables: a function per step and a printed assumptions table.

## Core calculations

- **Unit economics**: CAC = acquisition spend ÷ new customers acquired from it (same period, same channel). Contribution margin per customer = revenue − variable costs (COGS, payment fees, support). LTV = monthly contribution margin × expected lifetime, where lifetime ≈ 1 ÷ monthly churn only for steady churn; prefer cohort-based cumulative margin. Payback months = CAC ÷ monthly contribution margin. LTV:CAC is meaningless without the margin basis and time horizon; state both.
- **Forecasts**: build revenue from drivers (customers × price × retention, or traffic × conversion × order value), not a single growth rate. Tie headcount and costs to the plan that drives them. Show monthly for the first year.
- **Runway**: cash ÷ net monthly burn, recomputed month by month with the forecast (burn changes), not a single division.
- **Three statements**: net income flows to retained earnings; the cash-flow statement reconciles opening to closing cash; the balance sheet balances every period. A balance check row that is not zero is a bug.
- **DCF**: free cash flow to firm, discounted at WACC; terminal value by perpetuity growth (g below long-run nominal GDP growth, and below WACC) or exit multiple, and show what share of value the terminal carries. Mid-year convention stated. Equity value = enterprise value − net debt.

## Scenarios and sensitivity

Always give base, downside and upside with the assumptions that differ between them. Build a sensitivity table on the two inputs that move the answer most (for example churn × CAC for payback, WACC × g for DCF). Point estimates without ranges overstate certainty.

## Double-check

- Recompute one output by hand from the inputs.
- Check identities: balance sheet balances, cash reconciles, segment totals sum to the whole.
- Check limiting cases: zero growth, zero churn, 100% churn behave sensibly.
- Check units: monthly vs annual churn and rates, percentages vs points, thousands vs units.
- Sanity-check against reality: implied market share, revenue per employee, margins vs comparable companies.

Models are analysis, not advice: state limitations, and never present a forecast as a guarantee. For published statements use `financial-statement-analysis`; for portfolios and market risk use `investment-risk-analysis`.
