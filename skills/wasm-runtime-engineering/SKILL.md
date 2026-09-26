---
name: wasm-runtime-engineering
description: "Engineer advanced WebAssembly memory, ABI, threading, components and host integration."
---

# WebAssembly Runtime Engineering

Use for cross-runtime Wasm architecture and difficult integration/performance issues. Existing wasm-c-cpp, wasm-rust and wasm-python guides own language-specific build recipes.

## Working method

- Establish host/runtime version, target features, imports and security boundary.
- Define memory ownership, strings, errors and cancellation across the ABI.
- Measure transfer, startup and host-call costs alongside kernel time.
- Test feature detection, fallback and adversarial resource limits.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
