---
name: c-systems-engineering
description: "Write portable C with explicit memory, integer, ABI and POSIX contracts, including numerical kernels."
---

# C Systems Engineering

Use for C implementation, native libraries and Linux-facing code. Pin the language standard and compiler targets; a C++ compiler accepting a file does not verify C portability.

## Working method

- Document ownership, buffer length, alignment, representation and error contracts.
- Check arithmetic before allocation or indexing.
- Keep portable computation separate from OS/ISA-specific adapters.
- Validate with warnings, sanitizers where supported, boundary cases and an independent numerical baseline.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
