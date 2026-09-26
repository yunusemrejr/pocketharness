---
name: embedded-device-engineering
description: Build and debug ESP, Arduino and Raspberry Pi firmware or peripherals with target-specific compile checks, serial/GPIO safety and bounded hardware verification. Use for board bring-up, flashing and embedded I2C/SPI/UART work, not general Linux administration or ML model optimization.
---

# Embedded device engineering

Establish the host, project and physical target before choosing a command. An ESP board, a Raspberry Pi Linux computer and an RP2040/RP2350 microcontroller require different toolchains and recovery paths.

## Discover without touching hardware

- Use `sys_probe {action:"host"}` for resources, session/network/power indicators and toolchain presence, `sys_probe {action:"devices"}` for device-node candidates, and `project_report {view:"workspace"}` for configuration evidence. These describe the current process's visible environment, which may be a container; they do not identify a remote board or prove safe access.
- Inspect the existing board/FQBN, SDK version, pin map, partition layout and build scripts. Reuse the project toolchain. Check custom targets, pre/post hooks and PlatformIO extra scripts before running a purported build/test; they can upload or execute host commands.
- Keep the controlling machine's Wi-Fi, network route, terminal, parent processes and power intact. Remote SSH or desktop access may depend on them. Do not disable host protections to make a test pass. Use existing collaboration coordination to establish who owns the board/serial port; a process snapshot alone is not a lock.

## Implement and compile first

Choose the smallest change in the existing driver, task or state machine. Separate pure parsing/control logic from hardware I/O so host fixtures can cover frame lengths, endianness, CRC/checksum failures, partial reads, timeouts and disconnect/reconnect behavior.

Bound buffers, queues, retries and log volume. Use nonblocking state transitions with wrap-safe timer arithmetic. Keep interrupt handlers short and defer blocking I/O, allocation and logging; use the SDK's interrupt-safe synchronization, because `volatile` is not an atomicity guarantee. Preserve watchdog coverage, cleanup and safe actuator states across errors and resets. Measure stack, heap, flash and timing against the actual board budget; do not assume desktop resource availability.

Run the narrow compile-only target and relevant host tests. Keep compile, upload and monitor as separate actions. A build success does not demonstrate electrical correctness or behavior on a board. Do not run upload-capable test targets by default.

## Execute a scoped hardware operation

Before flash, erase, reset, monitor or bus writes, establish the exact board revision/chip, port, firmware artifact and operation already authorized by the task. Resolve missing identity or authority before that action; existing explicit authorization does not need to be requested again.

Check the board documentation for pin numbering, voltage/current limits, power source, shared grounds, boot straps and connected actuators. Confirm recovery access and a compatible backup when supported; keep firmware dumps and device credentials private. Never guess pins, flash offsets, flash size, security fuses or voltage from a family name. Irreversible fuse/security changes and whole-chip erase require specific scope.

Use the existing managed shell and installed vendor tooling for the exact target. Serial monitoring and discovery can toggle reset lines; do not treat opening a port as passive inspection. Bound duration, retries and output; reserve the port for one writer. After a failed attempt, inspect the failure before retrying, stop on unexpected resets/power loss/identity changes, and do not escalate to erase or disable watchdogs automatically.

Verify the selected artifact, boot/version evidence and task-specific behavior with a bounded smoke test. Report separately what compiled, what was flashed and what actually ran; identify untested physical behavior and leave the device in the agreed safe state.

Consult the installed-version [Arduino CLI build/upload documentation](https://docs.arduino.cc/arduino-cli/getting-started), [ESP-IDF monitor reset behavior](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-monitor.html), or [Raspberry Pi hardware and GPIO documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html) when those operations apply. Use the exact board datasheet for electrical limits and alternate pin functions.
