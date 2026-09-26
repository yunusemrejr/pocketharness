# WebAssembly Runtime Engineering: patterns and examples

## Memory and boundary contracts
Linear memory pointers are offsets, not host pointers. Check `ptr <= size` and `len <= size - ptr` to avoid overflow in bounds validation. A valid range can still contain invalid encoding or violate application ownership. Memory growth can invalidate or change host views: reacquire typed-array/DataView access after calls that may grow memory, accounting for shared versus unshared memory behavior. Never retain borrowed memory across an unknown allocating call without a lifetime contract.

For a buffer-returning API define allocator, length unit, alignment, free function and error ownership. JS strings are not UTF-8 byte arrays. Wasm i64 integration, memory64 pointer handling and BigInt support depend on the host ABI; do not truncate addresses into 32-bit JS bitwise operations. Zero-copy can retain huge allocations or introduce races; copying a small payload may be the safer faster end-to-end choice.

## Threads, components and capabilities
Browser shared memory needs supported isolation/security configuration; verify COOP/COEP and cross-origin asset behavior rather than adding headers blindly. Blocking waits on the browser main thread are not a portable design. Atomics synchronize memory; they do not make arbitrary host objects thread-safe. Component-model/WIT resources, WASI versions and async support have distinct runtime contracts; a core Wasm module is not automatically a portable component.

Imports grant abilities. Restrict filesystem/network capabilities and bound memory, fuel/epoch execution or wall time using actual runtime support. A sandbox does not prevent an authorized import from doing harmful work. Validate reentrancy when the host calls back into a module.

## Optimization and evidence
Batch boundary calls; benchmark scalar/SIMD variants with transfer and compilation included. Feature-detect SIMD, threads, exceptions, GC or memory64 on intended runtimes; keep meaningful fallback where required. A language's fast-math flags can change numerical promises. Test growth, empty buffers, malformed UTF-8, oversized lengths, cancellation and trap cleanup. Pin a reproducible build and inspect imports/exports and binary size. Runtime feature tables are versioned evidence, not universal compatibility guarantees.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://webassembly.org/features/
- https://webassembly.org/docs/portability/
- https://github.com/WebAssembly/component-model
- https://docs.wasmtime.dev/
- https://developer.mozilla.org/en-US/docs/WebAssembly
