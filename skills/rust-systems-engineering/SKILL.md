---
name: rust-systems-engineering
description: "Build Rust services, CLIs and native libraries with explicit ownership, errors and safe FFI."
---

# Rust Systems Engineering

Use for Rust implementation and review. Keep unsafe code and platform assumptions at small, documented boundaries.

## Working method

- Inspect edition, minimum supported compiler, features and target.
- Model ownership and error recovery before reaching for Arc, clones or unsafe.
- Bound async work and cancellation; verify dropped futures do not lose committed work.
- Test public contracts, feature combinations and unsafe invariants.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
