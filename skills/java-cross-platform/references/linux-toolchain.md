# Linux toolchain

## JDK selection

Decide the major version from the project's target first, then pick a distribution that ships it; verify current availability from the vendor's own documentation rather than assuming a distribution still ships a given version. Common choices are the distribution OpenJDK package, a Temurin-style binary, or a vendor JDK when support terms require one. Record the exact `java -version` output in the project notes so a later machine can reproduce the runtime.

Keep one JDK per project explicit: `JAVA_HOME` for the build, wrapper-pinned build tools, and container images for CI. `update-alternatives` manages the system default on Debian-style systems, but a project build must not depend on the ambient default. Gradle toolchains and Maven toolchains can select a JDK by version; verify the selected toolchain in build output instead of assuming the daemon reused the right one.

## Build entry points

Use the project's wrapper (`mvnw`, `gradlew`) so the build tool version is pinned. A wrapperless `mvn`/`gradle` from PATH is a reproducibility risk: record its version when you must use it. Keep the wrapper jar and properties in version control; they are part of the build contract.

Check offline behavior deliberately: dependency caches, plugin resolution, and Gradle configuration cache can all hide network dependence. A clean build from an empty cache on a fresh checkout is the only honest portability signal for the Linux side.

## Runtime images with jlink

`jlink` builds a trimmed runtime containing only the modules the application needs. Start from `jdeps` to find module dependencies, then build the image and run the packaged application against it — not against the full development JDK. Native service behavior (TLS providers, charsets, locales, timezone data) can differ between the full JDK and a stripped image; include `jdk.crypto.ec`, locale data, or other modules the application actually touches.

A jlink image is platform-specific: a Linux image never runs on Windows or macOS. Build each target image on (or for) its target OS, and keep the module list shared so the images differ only by platform.

## Common Linux failure signatures

- Headless AWT or font failures in containers: the image lacks font configuration or headless libraries. Decide whether the workload genuinely needs AWT or should run headless, then add exactly that dependency.
- `UnsatisfiedLinkError` for a bundled `.so`: wrong architecture, missing glibc version, or musl (Alpine-style) versus glibc mismatch. Check `ldd` output and the library's documented platform matrix.
- Locale or timezone drift between developer machines and CI: pin `user.language`, `user.country`, and `user.timezone` explicitly for tests rather than inheriting the container default.
- File-watch or memory-mapped Oddities on network filesystems: verify with the real mount type, not only a local ext4 checkout.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://dev.java/learn/
- https://docs.oracle.com/en/java/
- https://maven.apache.org/guides/
- https://docs.gradle.org/current/userguide/userguide.html
