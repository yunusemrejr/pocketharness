---
name: dotnet-linux-engineering
description: "Build C# and .NET software for Linux with portable dependencies, async ownership and deployment checks."
---

# C# and .NET on Linux

Use for C# services, tools and cross-platform applications. Check framework and native dependency support rather than assuming Windows behavior.

## Working method

- Inspect target framework, SDK pin and runtime identifier.
- Model nullability, disposal and cancellation explicitly.
- Check filesystem, native library and GUI portability.
- Test the published artifact on the intended Linux target.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
