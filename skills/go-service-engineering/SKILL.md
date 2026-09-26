---
name: go-service-engineering
description: "Implement Go services and command-line tools with bounded concurrency and explicit resource ownership."
---

# Go Service Engineering

Use for Go code and Linux deployment; distinguish the language from the ordinary verb “go”.

## Working method

- Inspect go.mod, toolchain and deployment constraints.
- Keep interfaces small and at the consuming boundary.
- Give each goroutine an owner, termination path and bounded work queue.
- Verify errors, cancellation, races and shutdown behavior.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
