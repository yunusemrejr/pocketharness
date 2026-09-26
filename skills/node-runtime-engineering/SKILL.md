---
name: node-runtime-engineering
description: "Build Node.js services, CLI and TUI tools with bounded async work, safe process handling and reproducible packages."
---

# Node Runtime Engineering

Use for JavaScript/TypeScript running in Node, including APIs, workers, CLIs and TUIs. Confirm the actual supported runtime rather than borrowing browser assumptions.

## Working method

- Inspect package type, engine constraints, lockfile and entrypoint.
- Assign ownership for promises, streams, subprocesses and shutdown.
- Bound concurrency, input size and retries at one layer.
- Test packaged execution, cancellation and production startup.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
