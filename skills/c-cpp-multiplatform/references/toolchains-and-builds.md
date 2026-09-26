# Toolchains and builds

## Compilers per target

GCC and Clang cover Linux and macOS hosts and most cross targets; MSVC (or Clang-CL) covers native Windows. Do not assume flag compatibility: warning flags, sanitizer support, and C++ standard conformance differ per compiler and version. Pin the compiler version per target and record it with the build evidence.

Prefer the same warning discipline everywhere: a strict warning set with warnings-as-errors for project code, while keeping third-party headers out of that policy (system-include treatment or explicit suppression). A warning that appears only on one target is still a defect signal.

## CMake presets and toolchain files

Use `CMakePresets.json` so each target is one named preset: compiler, generator, build type, and toolchain file. A toolchain file sets the system name and processor, the compiler paths, the sysroot, and search modes (`CMAKE_FIND_ROOT_PATH` with program/library/include modes) so host tools never leak into target builds.

Verify the triple the compiler actually targets (`<compiler> -dumpmachine` or the equivalent verbose output) after configuring; a preset that silently configured for the host is a false cross build. Keep one shared module of project options (standard level, warnings, sanitizers where supported) included by every preset so targets differ only by platform.

## Sysroots and dependencies

A sysroot pins the target's headers and libraries. Build or fetch it reproducibly and record its provenance; a hand-copied sysroot from an unknown device image is not a contract. For vendored dependencies, prefer the project's documented package path (submodule, package manager, or system package) and verify it resolves on every target — a dependency available only via one OS package manager breaks the matrix.

Check C library compatibility: glibc versions, musl versus glibc, and Windows CRT linkage (static versus dynamic) all change what runs where. Link and launch on the oldest supported target configuration, not only the developer machine.

## Device targets and emulators

Small or ARM devices add constraints beyond the OS: CPU features (NEON versus SSE, atomics support), memory ceilings, slow filesystems, and missing peripherals. Gate optional acceleration behind runtime feature detection with a portable fallback, and keep the fallback tested — an untested fallback is dead code.

Emulators (QEMU user-mode, vendor simulators) reproduce instruction behavior, not timing, power, or peripheral quirks. Use them for functional test runs, then verify performance-sensitive and hardware-adjacent behavior on real devices.

## CI per target

Run one CI job per matrix cell: configure the preset, build, and run the target's test scope (native execution, emulation, or device farm). Cross-compiling without running tests proves only that the code compiles. Keep build logs with compiler versions and flags; when a target fails, the log must identify the cell without guesswork.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://cmake.org/cmake/help/latest/
- https://clang.llvm.org/docs/CrossCompilation.html
- https://gcc.gnu.org/onlinedocs/
- https://learn.microsoft.com/en-us/cpp/
