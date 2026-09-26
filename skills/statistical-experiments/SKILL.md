---
name: statistical-experiments
description: Design and analyze controlled experiments, uncertainty estimates and causal comparisons; use for A/B tests or deciding whether a measured improvement is real.
---

# Statistical Experiments

Define the estimand: population, intervention/comparison, outcome, observation window and unit of analysis. Separate descriptive association from a causal conclusion.

1. Establish the assignment mechanism and likely confounders. For randomized work choose the unit that avoids interference; repeated measurements from one user are not independent users.
2. Set the primary outcome, practical effect threshold and stopping rule before looking for significance. Keep exploratory findings labeled exploratory.
3. Check missingness, attrition, sample-ratio mismatch, time trends and contamination. Explain exclusions; do not silently filter inconvenient outcomes.
4. Estimate effect size and uncertainty using a method compatible with sampling, clustering and distribution. Resample at the independent unit when bootstrapping. Correct for planned multiple comparisons when relevant.
5. Report counts, denominators, interval assumptions and robustness checks. Nonsignificance is not proof of equivalence; observational adjustment alone does not establish causality.

Example: comparing request latency across two versions can be confounded by different traffic mixes. Match workload or randomize assignment before attributing the change to code.

## A/B test mechanics that decide most outcomes

- **Power before launch**: from the baseline rate, the minimum effect worth acting on and the traffic, compute the sample size and duration (two-proportion test: n per arm ≈ 16·p(1−p)/δ² at 80% power, α=0.05). If the duration is unrealistic, test a bolder change or a higher-traffic metric instead of running an underpowered test.
- **Sample-ratio mismatch**: check the observed split against the intended one with a chi-square test first; SRM (p < 0.001) means broken assignment or logging, and the result is not interpretable.
- **No peeking**: fixed-horizon tests are read once at the planned sample. If continuous monitoring is needed, use a sequential method (group-sequential boundaries or always-valid intervals) chosen up front.
- **Full weeks**: run whole weekly cycles; novelty and day-of-week effects distort short tests.
- **Variance reduction**: CUPED (pre-period covariate adjustment) or stratification shrinks intervals without more traffic; state it when used.
- **Ratio metrics** (revenue per session, CTR per user) need the delta method or bootstrap at the randomization unit.
- **Many metrics or variants**: name one primary metric; correct secondary ones (Holm or Benjamini–Hochberg) or label them exploratory. Guardrail metrics (errors, latency, refunds) must not regress.
- **Report**: absolute and relative effect with a 95% interval, sample per arm, duration, SRM check result and a ship/iterate/stop decision tied to the practical threshold.

Deliver the analysis script and a decision justified by practical effect and uncertainty, not a p-value alone. Do not manufacture sample size or confidence intervals. For general exploratory analysis use `data-analysis`.
