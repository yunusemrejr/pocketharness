---
name: linux-host-defense
description: "Harden Linux hosts against untrusted inputs, exposed services and privilege escalation without breaking access."
---

# Linux Host Defense

Use for a requested security review or a concrete trust-boundary problem on Linux devices and servers. Routine navigation belongs to ubuntu-operations. Identify the actual machine and service before changing policy.

## Working method

- Inventory exposed listeners, identities, privileges, updates and recovery access.
- Trace untrusted data to shell execution, file writes, dependencies and service credentials.
- Choose the least disruptive control at the actual boundary; preserve recovery access.
- Verify both blocked hostile behavior and the legitimate workflow.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
