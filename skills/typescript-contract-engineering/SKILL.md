---
name: typescript-contract-engineering
description: "Engineer TypeScript contracts across frontend, Node, TUI and package boundaries."
---

# TypeScript Contract Engineering

Use for TypeScript implementation and migrations. Pair with browser or Node guidance when runtime behavior matters.

## Working method

- Inspect tsconfig, module resolution, runtime and public package exports.
- Represent real states with discriminated unions and validate unknown inputs.
- Keep type assertions and generic abstractions narrowly justified.
- Test emitted/package behavior as well as type checking.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
