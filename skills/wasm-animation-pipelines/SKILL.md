---
name: wasm-animation-pipelines
description: Build C++ and WebAssembly animation or physics pipelines with stable JS memory contracts, fixed simulation steps, batched transforms, SIMD, workers and rendering handoff. Use when animation computation crosses a Wasm boundary or needs native-kernel performance.
---

# Wasm animation pipelines

Choose Wasm for measured compute or native-library reuse. Establish the browser/renderer boundary, coordinate units, simulation clock and memory owner before optimizing a kernel. Preserve the scalar result as the correctness reference.

- For C++/JS contracts, handles, allocation, fixed steps and view invalidation, read [ABI and simulation](references/abi-and-simulation.md).
- For toolchain flags, SIMD, threads, OffscreenCanvas and performance decisions, read [execution and budgets](references/execution-and-budgets.md).

Do not migrate DOM work into Wasm or enable threads merely because the scene animates. Batch state exchange, report failures as explicit status values, and retain a functioning fallback appropriate to the target browsers. Separate numerical equivalence from visual plausibility when validating a port.
