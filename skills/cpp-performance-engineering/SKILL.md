---
name: cpp-performance-engineering
description: "Engineer C++ numerical, rendering and ML systems with RAII, precise lifetime contracts and measured optimization."
---

# C++ Performance Engineering

Use for modern C++ systems, numerical libraries and rendering/ML hot paths. Establish the supported standard and ABI before choosing language or library features.

## Working method

- Make ownership and lifetime visible through RAII and interfaces.
- Preserve invariants across move, exception, cancellation and concurrency paths.
- Start optimization from a correct baseline and measured bottleneck.
- Verify numerical behavior, memory safety and realistic performance separately.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
