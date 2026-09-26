---
name: physics-modeling
description: "Formulate and validate mechanics, electromagnetism, quantum and thermal physics models with units and assumptions."
---

# Physics Modeling and Validation

Use for physics derivations and simulations. Match the governing approximation to the regime; a visually plausible simulation is not physical validation.

## Working method

- State coordinates, units, boundary/initial conditions and approximation regime.
- Derive a small analytic or limiting case before numerical work.
- Check conservation, residuals, stability and convergence.
- Separate physical uncertainty from discretization and floating-point error.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
