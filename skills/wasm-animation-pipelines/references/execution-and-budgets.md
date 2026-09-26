# Execution, toolchain and budgets

## Build variants

Record `emcc --version` and pin the SDK used for release. A template for a C++ library with the named exports actually implemented is:

```sh
em++ simulation.cpp -O3 -flto --no-entry -sMODULARIZE=1 -sEXPORT_ES6=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS='["_world_create","_world_destroy","_world_step","_world_snapshot"]' \
  -o simulation.mjs
```

Export only the functions/runtime helpers the JS wrapper uses. `ALLOW_MEMORY_GROWTH` controls linear memory; `ALLOW_TABLE_GROWTH` controls the indirect function table. Choose initial/maximum memory from measured workload and failure behavior. Compare `-O2`, `-O3` and `-Oz` for speed/size instead of inventing universal speed percentages. LTO requires compatible objects/toolchain, not emrun. Use documented debug flags such as `-gsource-map`; verify exact flags against the installed SDK. [Compiler flags](https://emscripten.org/docs/tools_reference/emcc.html), [settings](https://emscripten.org/docs/tools_reference/settings_reference.html).

## Layout and SIMD

A structure of arrays (positions x/y/z, velocities x/y/z) often vectorizes simple per-component loops; array of structures may be better for entity-local traversal. Benchmark the actual kernels. Preserve aligned accesses required by the C++ type, even though some Wasm loads permit unaligned addresses. Padding and false sharing are different concerns. An f32x4 SIMD operation processes four lanes; loop over complete groups then handle the tail without out-of-bounds reads. `-msimd128` enables Wasm SIMD; supported x86 intrinsic translation is selective and can expand into slower instruction sequences. Do not add `-mavx2` as a universal accelerator. [SIMD porting guide](https://emscripten.org/docs/porting/simd.html).

Changing floating-point reassociation or fast-math may change constraints, NaN checks and reproducibility. Compare tolerances and conservation before enabling it. Keep a scalar module if the deployment requires a fallback; detect the actual Wasm feature before loading a SIMD-only binary. Feature detection is not evidence of speed.

## Threads and snapshot publication

Emscripten pthread builds use `-pthread` at compile and link. Browser shared-memory execution requires an appropriate secure, cross-origin-isolated environment. A common document configuration is COOP `same-origin` and COEP `require-corp`; embedded resources must satisfy the resulting CORS/CORP policy. Headers are a deployment decision, not an instruction to attach identical headers blindly to every response. Check `crossOriginIsolated` and module startup; provide a separate single-thread build when needed. `PROXY_TO_PTHREAD` changes where main runs and is optional. Never block the browser main thread waiting for worker progress. [Pthreads](https://emscripten.org/docs/porting/pthreads.html).

Use explicit buffer ownership for render snapshots: producer claims FREE, writes payload, then publishes READY through an atomic control word; consumer atomically claims READY as READING, reads, then releases FREE. Only the owner touches that payload. A two-buffer swap without acknowledgement can let the producer lap a slow consumer. A sequence counter alone does not make concurrent non-atomic C++ payload reads/writes legal. Prefer keeping C++ atomic synchronization within C++ and expose a documented JS handoff protocol rather than assuming arbitrary C++ object/atomic layouts. For a JS/Wasm shared control ABI, specify aligned integer words, memory ordering and ownership transitions, and test stress interleavings.

## Worker and canvas lifetime

`transferControlToOffscreen()` must occur before obtaining a rendering context on the HTML canvas, and the canvas can be transferred only once. Send the OffscreenCanvas in the worker transfer list. The main thread retains DOM input/layout; the rendering owner handles the graphics context. Forward bounded input and resize state, including drawing-buffer dimensions. Use generation IDs to ignore stale results after teardown. Stop rendering and release resources before terminating the worker where practical; replace the HTML canvas if a remount requires a new transfer. A transferred canvas cannot simply revert to ordinary main-thread rendering; decide the fallback before transfer or recreate the element. [Offscreen transfer](https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/transferControlToOffscreen).

## Cost arithmetic

**Worked snapshot budget.** 50,000 transforms with position (3), quaternion (4), scale (3) float32 values occupy `50,000*10*4 = 2,000,000 bytes`. At 60 snapshots/s this is 120 MB/s payload. Three snapshot slots occupy 6 MB, excluding simulation state and GPU copies. Sending positions only costs 600,000 bytes/snapshot, or 36 MB/s. Choose a reduced layout only if the renderer can reconstruct missing fields correctly.

**Worked optimization decision.** Suppose measured JS simulation=6 ms, rendering/other=4 ms, and a proposed Wasm kernel=2 ms plus packing/copying=3 ms: total falls from 10 to 9 ms, only 1.11× faster, not 3×. Batching that reduces boundary cost to 0.4 ms yields 6.4 ms total, about 1.56×. These illustrative numbers are not benchmarks. Measure warm and cold startup, transfer/allocation cost and p95 frame time on target devices.

At 60 Hz the entire presentation interval is 16.67 ms; at 120 Hz it is 8.33 ms. CPU and GPU can overlap, so do not blindly add independent GPU and CPU timers. Worker throughput can improve while input-to-visible latency gets worse from queued snapshots. Limit queue depth and document whether newest-state replacement is allowed. For deterministic recording, preserve frames with explicit backpressure instead of silently dropping them.

Sources checked 2026-09-09; confirm behavior against the installed SDK and actual browser capabilities, not a static browser-version table.
