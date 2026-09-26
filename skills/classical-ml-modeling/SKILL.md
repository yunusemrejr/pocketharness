---
name: classical-ml-modeling
description: "Develop reliable tabular and classical ML with leakage-safe pipelines, calibration and appropriate validation."
---

# Classical ML Modeling

Use for regression, classification, clustering and feature-based prediction. Favor an interpretable baseline before increasing complexity.

## Working method

- Define target, prediction time, available features and error cost.
- Split by time/entity before fitting any preprocessing.
- Compare baseline models and tune only inside training/validation.
- Check calibration, subgroup behavior and deployment drift.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.

## Dependency-free baseline

[logistic_regression.cpp](assets/logistic_regression.cpp) is a small C++17 binary classifier for two numeric features. It fits train-only means/scales and an L2-regularized logistic model, then reports held-out accuracy, log loss, Brier score and a training-prior baseline. From this skill's directory:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Werror assets/logistic_regression.cpp -o "$TMPDIR/logistic-regression"
"$TMPDIR/logistic-regression" --self-test
"$TMPDIR/logistic-regression" train.csv heldout.csv
```

Each CSV has no header and exactly `feature_1,feature_2,label` per row; label is integer 0 or 1. Empty data, malformed rows, nonfinite features and single-class training are rejected. Features must have magnitude at most `1e100`; each file is capped at 100000 rows. This is a readable fixed-configuration baseline, not a general trainer or calibrated decision system. It does not handle missing values, categories, class weighting, model export or streaming updates.

Choose and freeze the split before running it: separate entities or time periods when rows are related. Use a separate validation partition for any tuning; never tune on the reported holdout. Acceptance: compare held-out log loss to the prior baseline, inspect relevant class/error costs, and check preprocessing cannot change during evaluation. The seeded synthetic self-test verifies implementation behavior only. Keep an existing project's established ML stack for larger datasets or richer models; this example adds no Python or ML-library requirement to PocketHarness.
