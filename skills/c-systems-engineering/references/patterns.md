# C Systems Engineering: patterns and examples

## Memory and arithmetic
Use size_t for sizes while checking signed conversions. Signed integer overflow is undefined behavior; unsigned wrapping is not automatically correct. Check `n > SIZE_MAX / sizeof *p` before allocating n elements. Preserve the original pointer when realloc fails. A pointer plus a length is still unsafe if the length does not describe the actual object. Null termination, embedded NUL and byte length are separate contracts.

```c
if (n > SIZE_MAX / sizeof *items) return ERR_SIZE;
void *replacement = realloc(items, n * sizeof *items);
if (n != 0 && replacement == NULL) return ERR_MEMORY;
items = replacement;
```
Handle n==0 according to the project's ownership contract; realloc zero-size behavior is not a convenient portable free-and-reuse primitive across standards.

## Interfaces and Linux
Specify who allocates/frees across shared-library or FFI boundaries, including allocator compatibility. Do not expose compiler-dependent struct layout as a wire format. Serialize fields with explicit widths/endian handling; padding bytes may be uninitialized. POSIX read/write can be short or interrupted; retry only according to operation semantics and preserve progress. Close descriptors on every path, set close-on-exec where appropriate and distinguish descriptor ownership from borrowed FILE pointers.

Volatile is not thread synchronization. Atomics require a happens-before argument, not just an instruction that appears indivisible. Prefer simple mutex ownership unless lock-free behavior is justified. Signal handlers are restricted environments; defer ordinary work.

## Numerical kernels
Define dtype, stride, aliasing and alignment. `restrict` is a promise, not an optimization request: violating it can miscompile. Use a scalar reference before SIMD/BLAS integration, test non-multiple tails and unaligned buffers, and compare residuals with scale-aware tolerance. FMA and reassociation change rounding; fast-math can invalidate NaN/Inf and conservation assumptions. Benchmark allocation, copies and compute separately, with realistic working sets.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.open-std.org/jtc1/sc22/wg14/
- https://man7.org/linux/man-pages/
- https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
