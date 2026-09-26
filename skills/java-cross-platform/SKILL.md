---
name: java-cross-platform
description: "Develop Java on Linux and ship it to Windows and macOS: JDK selection, Maven/Gradle wrappers, jlink runtime images, jpackage installers, path/locale/native-library portability and per-OS verification."
---

# Java Cross-Platform on Linux

Use when Java work starts on Linux but must also run on Windows or macOS, or when OS-specific behavior (paths, line endings, packaging, native libraries) causes failures. For JVM concurrency, transactions and persistence design use java-platform-engineering; for running Windows applications on Linux rather than shipping Java to Windows, use windows-on-linux-engineering.

## Working method

- Pin the JDK target and the build entry point first: which JDK distribution and version, Maven or Gradle wrapper, and the module or classpath layout. A build that works only in one IDE is not a portable build.
- Keep OS differences behind explicit seams: `java.nio.file.Path` instead of string paths, explicit charsets, explicit time zones, and per-OS dependency classifiers or runtime images rather than ambient machine state.
- Choose the shipping form per target OS: executable jar, jlink runtime image, or jpackage installer. Each form changes what must be tested on the target machine.
- Verify on every OS you claim: launch, file locations, native calls, and at least one failure path. Linux success is not evidence for Windows or macOS behavior.

Read [Linux toolchain](references/linux-toolchain.md) when setting up or diagnosing the Linux-side JDK, build and runtime image. Read [multi-OS packaging and portability](references/multi-os-packaging.md) when preparing Windows or macOS artifacts or fixing cross-OS defects; do not load it for Linux-only service work. User instructions take precedence; this skill adds no authority to install toolchains or publish releases.

## Evidence and completion

Report the JDK version, build command, artifact form, and which operating systems were actually exercised with what result. Name any OS-specific assumption (path layout, bundled runtime, native library) and what remains unverified. Do not present a Linux-only test run as multi-OS verification.
