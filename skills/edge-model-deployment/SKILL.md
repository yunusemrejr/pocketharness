---
name: edge-model-deployment
description: "Deploy and optimize ML on microcontrollers, SBCs and heterogeneous CPUs/accelerators with measured budgets."
---

# Edge Model Deployment

Use for ESP/Arduino-class devices, Raspberry Pi, Intel/AMD systems and Apple silicon. Match the exact board, chip, runtime and operators rather than assuming a vendor-wide capability.

## Working method

- Build a hardware/runtime/operator/dtype compatibility matrix.
- Budget peak memory, latency, energy and quality before selecting a model.
- Match calibration and preprocessing to real device inputs.
- Compare on-device results with a reference and measure sustained operation.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
