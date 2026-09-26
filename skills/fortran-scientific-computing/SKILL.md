---
name: fortran-scientific-computing
description: "Develop Fortran numerical software with explicit precision, array contracts and reproducible validation."
---

# Fortran Scientific Computing

Use for scientific kernels, legacy modernization and numerical libraries. Preserve validated physics and numerical contracts during refactors.

## Working method

- Identify standard, compiler flags, precision model and library ABI.
- Use modules and explicit interfaces; inspect array shape and contiguity.
- Test numerical residuals and bounds before optimizing loops.
- Record compiler, threading and floating-point settings with measurements.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
