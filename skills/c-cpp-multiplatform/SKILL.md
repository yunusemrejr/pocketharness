---
name: c-cpp-multiplatform
description: "Write portable C and C++ for multiple devices and operating systems: cross-compilation, GCC/Clang/MSVC toolchains, CMake presets and toolchain files, integer/ABI/filesystem contracts, and per-target verification."
---

# C and C++ Multiplatform

Use when C or C++ must build and run on more than one OS, architecture, or device class — Linux, Windows, macOS, ARM boards, or constrained targets. For single-platform C ownership and undefined-behavior discipline use c-systems-engineering; for C++ lifetime design and measured optimization use cpp-performance-engineering; for browser delivery use wasm-c-cpp.

## Working method

- Name the target matrix explicitly: operating systems, architectures, compilers, and C/C++ standards. Every portability decision follows from that matrix; an unnamed target is an untested target.
- Drive builds through one portable entry point (CMake presets plus toolchain files, or the project's documented equivalent) so each target configures the same source with its own compiler, sysroot, and flags.
- Put platform differences behind feature detection and small adapters: preprocessor checks on compiler-provided macros, `static_assert` on type widths, and filesystem, thread, and network seams — never scattered `#ifdef` business logic.
- Verify per target: configure, build with warnings as errors, run the test suite on (or for) the target, and exercise at least one device-realistic input. A Linux x86_64 build proves nothing about ARM, Windows, or a small device.

Read [toolchains and builds](references/toolchains-and-builds.md) when setting up compilers, sysroots, or cross builds. Read [portability contracts](references/portability-contracts.md) when writing or reviewing portable code; do not load both for a single narrow fix. User instructions take precedence; this skill adds no authority to install toolchains or change device firmware.

## Evidence and completion

Report the target matrix, the compiler and flags per target, and which targets actually built and ran tests. Name any target-specific assumption (word size, endianness, path layout, available libraries) and what remains unverified. Do not present one target's green build as multi-device verification.
