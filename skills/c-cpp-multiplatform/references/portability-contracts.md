# Portability contracts

## Integers and data layout

Never assume `int`, `long`, or pointer widths: use `<cstdint>` types at boundaries and `static_assert(sizeof(...))` where layout matters. `long` is 64-bit on LP64 Linux/macOS and 32-bit on Windows LLP64 — this single difference breaks serialization, varargs, and printf formats. Use `PRId64`-style macros or type-safe formatting instead of `%ld` for fixed-width integers.

Check signedness of plain `char` (compiler-defined), enum sizes, bit-field layout, and struct padding before sharing memory layouts across targets. Serialize explicitly with documented endianness; never `memcpy` a struct onto the wire or into a file and read it on another device. Verify with a test that writes on one configuration and reads on another, or with golden byte vectors.

## Characters, paths, and files

Use UTF-8 internally and convert at OS boundaries; Windows APIs have distinct narrow/wide behavior and consoles have their own code pages. Prefer `std::filesystem` over string path manipulation: separators, case sensitivity, reserved names, maximum lengths, and symlink semantics all differ. Normalize line endings for text protocols and test files rather than assuming `\n`.

File locking, atomic rename, fsync durability, and memory-mapped behavior are OS contracts — verify the guarantee you rely on against each target's documentation, especially on device storage and network filesystems.

## Threads, time, and dynamic loading

Prefer standard threads, mutexes, and atomics over OS primitives; drop to native APIs only behind a small adapter with per-target tests. Check the memory-ordering requirements on weakly ordered ARM rather than assuming x86-TSO behavior transfers. Measure clocks with monotonic sources for intervals and wall clocks only for timestamps; resolution and epoch differ.

Shared libraries differ per platform: symbol visibility defaults, `.so` versus `.dll` versus `.dylib` loading, rpath/`@rpath`/DLL search order, and CRT or libc++ pairing on Windows. Control symbol visibility explicitly, document the library's runtime dependencies per target, and test loading from the installed layout — not just the build tree.

## Feature detection

Detect, do not assume: compiler feature-test macros (`__cpp_*`, `__has_include`, `__has_builtin`), CMake compile checks for headers and functions, and runtime CPU dispatch for SIMD paths. Keep fallback paths compiled and tested on every target; a fallback that only builds on one OS will rot silently.

Guard OS API usage with the project's minimum-version macros (for example `_WIN32_WINNT`, `__MAC_OS_X_VERSION_MIN_REQUIRED`, `_GNU_SOURCE` scope) and verify the minimum supported version still builds in CI. New-API adoption without a minimum-version job breaks old devices quietly.

## Portability review checklist

- Fixed-width integers at every serialization, IPC, and file boundary.
- No bare `long`, `char` signedness, or pointer-to-int assumptions.
- Paths through `std::filesystem`; no hardcoded separators or case assumptions.
- Explicit symbol visibility and documented shared-library dependencies.
- Every `#ifdef` platform branch built and tested in the matrix.
- New warnings on any target treated as failures.

## Primary references

- https://en.cppreference.com/w/
- https://cmake.org/cmake/help/latest/
- https://learn.microsoft.com/en-us/cpp/
- https://developer.apple.com/documentation/
