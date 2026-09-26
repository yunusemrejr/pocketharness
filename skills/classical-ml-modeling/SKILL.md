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
