---
name: x86-assembly-engineering
description: "Write and review x86-64 assembly, SIMD kernels and ABI boundaries with measured correctness."
---

# x86 Assembly and ABI Engineering

Use when x86 or x86-64 is confirmed. An .asm or .S suffix alone does not establish the instruction set.

## Working method

- Identify ISA, assembler syntax, ABI, privilege level and target CPU/OS.
- State register, stack, memory and flag contracts before writing instructions.
- Preserve a scalar reference and safe dispatch path.
- Inspect assembled output and test boundary sizes before benchmarking.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
