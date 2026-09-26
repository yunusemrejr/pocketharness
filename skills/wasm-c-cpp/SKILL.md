---
name: wasm-c-cpp
description: Compile C/C++ to WebAssembly with Emscripten, explicit JS ownership and interop, SIMD/threads, loading and size/performance diagnosis. Use for native ports and Wasm ABI or toolchain issues.
---

# C/C++ to WebAssembly

## Build against the installed SDK

Record `emcc --version`; use `em++` for C++ linking. For a program that defines main:

```sh
em++ main.cpp -O2 -sMODULARIZE=1 -sEXPORT_NAME=createApp \
  -sEXPORTED_FUNCTIONS='["_main"]' -o app.js
```

For a library, use `--no-entry` and export its real C ABI functions. Compare `-O2`, `-O3`, `-Os`/`-Oz` for the workload; avoid undocumented optimization flags or promised percentage wins. `-flto` can help with compatible compilation/linking; it does not require emrun. Use documented debug options such as `-gsource-map`. [Compiler reference](https://emscripten.org/docs/tools_reference/emcc.html).

`ALLOW_MEMORY_GROWTH=1` enables linear-memory growth. `ALLOW_TABLE_GROWTH` concerns the function table and will not fix heap exhaustion. Set initial/maximum memory from actual workload and handle allocation failure. Memory64 requires a deliberate pointer ABI and deployment compatibility check. [Settings](https://emscripten.org/docs/tools_reference/settings_reference.html).

C++ exceptions are distinct from setjmp/longjmp. Emscripten supports JavaScript-based exception handling with `-fexceptions` and Wasm exception handling with `-fwasm-exceptions`, subject to target support and SDK behavior; apply needed flags consistently at compile/link. Do not describe longjmp as the default C++ exception mechanism. [Exceptions](https://emscripten.org/docs/porting/exceptions.html).

## Memory and interop

Typical Emscripten modules expose a linear heap through typed arrays. A pointer is a byte offset, not an independently owned JS buffer. Pass length/capacity and document ownership. Free each allocation exactly once with its matching allocator; transfer of ownership means the former owner stops freeing it. Borrowed heap views expire on allocation movement, free or relevant memory growth. Reacquire the current heap view after calls that may grow memory. Copy deliberately when async consumers require stable data. [Interop](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html).

Export C functions explicitly (with `extern "C"` for C++ names) or use `EMSCRIPTEN_KEEPALIVE` as appropriate. Export runtime helpers such as `ccall`, `cwrap` or `UTF8ToString` only when used; heap views are not functions. For strings, specify UTF-8 size and ownership. Embind is useful for higher-level bindings, but borrowed typed-memory views still need lifetime rules, and owned wrapper objects may require `.delete()`.

Asyncify transforms eligible call paths to support suspension and adds costs; assess its actual call graph or use an explicit async state machine when that better fits the API. Avoid universal claims that it rewrites every function or is always unsuitable.

## SIMD, threads and animation

Enable Wasm SIMD with `-msimd128`; supported x86 intrinsics may lower differently, so inspect or benchmark important kernels. Do not add AVX flags or `-march=native` blindly. Use `-pthread` consistently for pthread builds. Browser threads require shared-memory support and a compatible cross-origin-isolated deployment; evaluate document COOP/COEP and resource CORS/CORP behavior. Main-thread blocking can deadlock browser-dependent work. Supply a separate single-thread variant when the target needs it. [SIMD](https://emscripten.org/docs/porting/simd.html), [pthreads](https://emscripten.org/docs/porting/pthreads.html).

For fixed-step animation, snapshots, stable handles, worker rendering and measured transfer budgets, use [wasm-animation-pipelines](../wasm-animation-pipelines/SKILL.md).

## Loading and other runtimes

Serve Wasm with `application/wasm` for streaming compilation. Match the emitted JS, Wasm and any worker assets from one build; do not delete worker/helper files without examining the generated loader. Version/cache assets coherently. Reuse compiled modules when appropriate, while tracking separate instance memories and imports. Size depends on linked functionality, data and debug information; a universal megabyte threshold does not diagnose bad flags.

WASI requires a compatible host that provides its imports; it is not browser DOM access or unrestricted POSIX. Choose a WASI target/version according to the deployment runtime and SDK, not a blanket Preview 1 preference. For browser deployment, check the actual required Wasm features and target devices; no static version table substitutes for those checks.

Check numerical equivalence, invalid input, allocation failure, repeated instantiation/cleanup and retained-view behavior according to the change. Profile boundary crossings and copying alongside kernel time before claiming a speedup.

Official API/flag sources checked 2026-09-09; confirm options against the pinned SDK before building.
