---
name: windows-on-linux-engineering
description: "Run complex Windows apps and DLLs on Linux without a VM: Wine architecture, DLL overrides, PE loading, prefixes, DXVK/VKD3D, debugging."
---

# Windows on Linux Engineering

Use for running Windows executables and DLLs on Linux through API
translation (Wine/Proton) instead of virtualization: no VM, no Windows
license, Win32 calls translated at the boundary while machine code runs
natively. For prefix/runner GUI management see winecharm.

## Working method

- Rule out hard limits first: kernel-mode drivers (most anti-cheat/DRM)
  and boot-time services have no Wine workaround.
- Default every DLL to Wine builtin; go native one module at a time via
  `WINEDLLOVERRIDES`, recording each override with its symptom.
- One prefix per app with a pinned runner; match bitness end to end
  (32-bit DLL needs a win32 path and 32-bit caller).
- Direct3D through DXVK (9/10/11) or VKD3D-Proton (12); prefer
  winetricks verbs over hand-copied DLLs.

Read [patterns and examples](references/patterns.md) for loader details,
debugging channels, and the verification bar.

## Evidence and completion

Report runner, architecture, overrides, and launch command. Done means
the app or DLL entry point completes its real task end to end.
