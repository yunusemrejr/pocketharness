---
name: hybrid-ml-systems
description: "Combine rules, classical models and neural networks with leakage-safe interfaces and measured ablations."
---

# Hybrid ML Systems

Use for cascades, ensembles, neuro-symbolic designs and mixed statistical/neural architectures. Add a component only when it has a distinct measured job.

## Working method

- Assign each component an input/output contract and owner.
- Train combinations without leaking targets through upstream predictions.
- Compare each component and the combined system with ablations.
- Validate uncertainty, fallback, latency and correlated failures.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
