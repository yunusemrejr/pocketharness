# Multi-OS packaging and portability

## Shipping forms per OS

Choose one form per target and test that form on that target:

- Executable jar: smallest artifact, requires a preinstalled compatible JRE on the target machine. Document the minimum Java version and verify the launch command on each OS, including machines where `java` is not on PATH.
- jlink runtime image: bundles a trimmed JRE, so no preinstalled Java is needed, but the image is built per OS and architecture. Keep one module list and produce one image per target.
- jpackage installer: produces native installers (deb/rpm on Linux, msi/exe on Windows, dmg/pkg on macOS) from an app image. Each installer type must be built on its own OS. Verify install, launch, uninstall, and upgrade over a previous version.

Never ship a Linux-built app image inside a Windows or macOS installer: launchers, file permissions, and bundled native libraries are OS-specific.

## Code-level portability

- Paths: use `java.nio.file.Path` and `Files` everywhere. Never concatenate separators, never assume case sensitivity, and never assume a drive-letter or leading-slash layout. Windows reserved names (`CON`, `NUL`, trailing dots) break files that work on Linux.
- Line endings and encodings: set `.gitattributes` for text files, and always pass an explicit `Charset` to readers, writers, and `String.getBytes`. The platform default charset differs across operating systems and JDK configurations.
- Line-based tooling output: normalize `\r\n` when parsing subprocess output on Windows.
- Time: use explicit `ZoneId` values; the system default zone differs per machine. Store instants in UTC and convert at the display boundary.
- User directories: resolve per-OS conventions (`user.home`, `%APPDATA%`-style application data, `~/Library`, XDG directories) through a small documented helper rather than hardcoding one layout.

## Native libraries

JNI libraries ship per OS and architecture (`.so`, `.dll`, `.dylib`). Load them with `System.loadLibrary` plus a documented `java.library.path`, and fail with a message that names the expected file per platform. The Foreign Function and Memory API changes what is idiomatic per JDK version; verify against the project's actual JDK instead of repeating older JNI-only guidance.

Check the C runtime the native library needs: a Windows DLL may require a specific Visual C++ redistributable, and a Linux `.so` may require a glibc version newer than the target distribution ships. These failures appear only on the target OS, never in a Linux-only build.

## Signing, notarization, and CI matrix

Windows and macOS gate unsigned software with warnings or blocks whose exact behavior changes over time; check the current platform requirements before promising a double-click install. Credentials for signing live in the OS environment or the CI secret store — never in the repository, build scripts, or logs; see systems-security for handling.

Run a CI matrix with one job per supported OS and architecture. Each job builds (or consumes the per-OS artifact), launches it, and runs the smoke suite including file-system and subprocess paths. A green Linux job plus an untested Windows artifact is not a multi-OS release.

## Primary references

- https://docs.oracle.com/en/java/javase/21/jpackage/packaging-tool-user-guide.html
- https://dev.java/learn/
- https://maven.apache.org/guides/
- https://docs.gradle.org/current/userguide/userguide.html
