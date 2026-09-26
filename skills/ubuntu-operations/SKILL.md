---
name: ubuntu-operations
description: "Operate Ubuntu desktops and servers safely using shell, packages, services, permissions and diagnostics."
---

# Ubuntu Operations

Use when a task depends on Ubuntu behavior, package installation, service diagnosis or Linux shell operations. Do not assume that a local path, root prompt or remote shell identifies the intended host.

## Working method

- Establish distro, user, working directory, host/container boundary and available commands.
- Prefer read-only inspection before changing packages, permissions, mounts or services.
- Make commands handle spaces, failure and cancellation deliberately.
- Validate the affected process or artifact, not merely command exit status.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
