# Windows-on-Linux patterns and examples

Primary references: https://www.winehq.org/docs (Wine documentation),
https://wiki.winehq.org/Debug_Channels (WINEDEBUG channels),
https://www.protondb.com (per-game Proton recipes), and
https://github.com/doitsujin/dxvk (Direct3D over Vulkan).

## Architecture facts that drive decisions

- Wine is a clean-room Win32 implementation plus a PE loader; `wineserver`
  owns cross-process state (registry, handles, synchronization) with one
  server per `WINEPREFIX`. WoW64 runs 32-bit apps on 64-bit Wine.
- API sets (`api-ms-win-*`) forward to real DLLs: stub warnings naming
  an api-set mean the target module needs attention, not the set.
- Proton adds DXVK/VKD3D, esync/fsync, and game fixes to Wine; check
  ProtonDB for a known recipe before inventing prefix configuration.

## DLL loading patterns

- Smoke-test an export with `rundll32 <dll>,<EntryPoint>`; for complex
  DLLs build a tiny C harness with `winegcc` (Winelib) around
  `LoadLibrary`/`GetProcAddress` so failures surface with symbols.
- Dependency-walk before executing: capture the loader error verbatim.
  `WINEDEBUG=+loaddll,+module` shows the resolved builtin/native choice
  per module; `+relay` is a last resort on small repros only.
- `WINEDLLOVERRIDES` order matters (`d3d11,dxgi=n,b` tries native
  first); never blanket-override — version mismatches fail far away.

## Prefix recipe record

Keep with each prefix: architecture, runner name and version, winetricks
verbs, full `WINEDLLOVERRIDES`, and the known-good launch command.
Upgrading a runner is a deliberate re-verification event: clone, upgrade,
re-run the app's real task, keep the old prefix until green.

## Common failure signatures

- Immediate exit with loader error: missing import or bitness mismatch.
- Black window / rendering garbage: wrong DXVK version or missing
  Vulkan drivers (`vulkaninfo` first), not CPU speed.
- Anti-cheat/DRM refusal: kernel driver requirement — stop and report,
  no prefix tuning fixes this.
- Works once then breaks: shared-prefix cross-contamination; split
  prefixes and re-verify each app alone.
