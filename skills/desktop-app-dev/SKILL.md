---
name: desktop-app-dev
description: Build cross-platform desktop apps with Electron, Tauri or native tools. Use for runtime selection, OS integration, packaging, updates and desktop security.
---


# Desktop App Development

Verify desktop behavior by running it: `desktop_session` launches the app into a private virtual display, then clicks, types, presses keys and takes screenshots, without touching the user's screen. Check the window appears (`wait` with its title), the main flows respond to input, and `logs` shows no startup errors.

## Choose the stack (decide first, it's the 90% decision)

| Need | Pick |
|---|---|
| Web team, large app, shipping now | **Electron** (mature, huge ecosystem, heavy: ~80–150 MB RAM base, ~80+ MB disk with runtime) |
| Web frontend, smaller footprint, Rust backend available | **Tauri v2** (webview + small Rust core: ~10–30 MB RAM overhead, ~3–10 MB app disk) |
| OS-native UX, performance-critical, no web frontend | native (SwiftUI/AppKit, WPF/WinUI, GTK/Qt) or Flutter desktop (less mature on Linux) |
| "We need a desktop app" but it's a SaaS page | web app + PWA/installable; a desktop wrapper adds install+update+signing costs |

The real axis: **how much do you want outside the browser** (tray, global shortcuts, local DB, file system, background presence). Small amount → Tauri/webview; full OS citizen → Electron or native.

## Electron essentials

- **Security is the default-off thing**: renderer with `contextIsolation: true` + `nodeIntegration: false` + `sandbox: true` always; all privileged work in the main process behind `ipcMain.handle`/`ipcRenderer.invoke`; expose a **minimal, typed, validated** API via `contextBridge` — the bridge surface is your attack surface.
- One window = one renderer; heavy work in `utilityProcess`/worker (parsing, ML, indexing) so the UI thread stays alive.
- `app.commandLine` / `app.disableHardwareAcceleration()` only with a reason (GPU driver class of bugs).
- Menu + native dialogs + notifications: use the built-ins; hand-rolled equivalents look wrong and break on one OS.
- asar by default; **no node_modules unpacked** in the app except native addons (`.node` files must be `asarUnpack`ed — classic "works locally, fails after pack" bug).

## Tauri v2 essentials

- Rust `#[tauri::command]` = the IPC surface: type-safe, and you own the security model via **capabilities** (file ACL, shell, windows — the app can only do what its capability grants). Default capabilities are least-privilege; expand deliberately per privilege.
- CSP is your first line: lock `script-src` to self; Tauri's init script and IPC are already scoped.
- Rust backend is a normal Rust app: you can host a local server (axum), talk SQLite (SQLx), spawn processes — the webview is just one frontend of it.
- Frontend stays a normal web app (Vite/Next) — dev flow: `tauri dev` (hot reload), `tauri build` (per-target release bundles).

## OS integration (both)

- **Tray + menu**, **global shortcuts** (Electron `globalShortcut`, Tauri global-shortcut plugin), **deep links** (register `scheme://` handler), **single-instance** (second launch focuses the window, passes its args — `app.requestSingleInstanceLock` / `single_instance` plugin).
- Launch-at-boot: OS API, not "start the app hidden" hacks (resurrects badly).
- Notifications: OS-native API (they respect user settings); don't roll toast in-app for OS-level events.
- File access: platform file dialogs; on Tauri, file reads go through the fs plugin **with path scope**, not arbitrary `fs::read`.
- Update: **built-in updaters** (electron-updater / Tauri updater plugin) with signed updates — your own "download new app and replace" is a security incident waiting to happen.

## Packaging & signing (the part that gets apps rejected/quarantined)

- **macOS**: Developer ID certificate (not App Store) → sign the whole app (Electron: electron-builder `osx.sign`; Tauri: `tauri.conf.json bundle.macOS.signingIdentity`) → **notarize** (xcrun notarytool / electron-builder `osx.notarize` / tauri notarize) → ship DMG (or ZIP for auto-update payload). Unsigned/non-notarized = Gatekeeper block or the scary "unidentified developer" dialog every time. (App Store = sandbox rules, different beast; pick one.)
- **Windows**: code-signing cert (OV for trust level, EV optional these days — SmartScreen now largely reputation-based; a small unsigned app gets a SmartScreen interstitial, which kills casual installs). electron-builder NSIS/MSI; Tauri NSIS/MSI/wiX.
- **Linux**: AppImage (portable, works) + .deb/.rpm or flatpak (distribution presence). No signing enforced by the OS (signature verification in flatpak = your repo config); sign with GPG for distro repos.
- Auto-update: signed updater payload + version + checksum; always keep the previous build reachable for rollback; update the app *files* on quit-or-restart cleanly (watch partial-write crashes: atomic replace, not in-place).

## UX & platform behavior

- Respect native conventions: Cmd/Win palette, `Cmd+,` settings, `Cmd+Q` quit (macOS keeps the app running after windows close — don't force-quit on last window unless you're mimicking a utility), context menus, hover states, window chrome insets for traffic lights (macOS `titleBarStyle: hiddenInset`).
- **Offline-first is the default** for desktop: local store (SQLite: better-sqlite3 / SQLx) as source of truth, server is sync endpoint; the app works on the plane.
- Settings live in the OS config store (Electron `app.getPath('userData')`, Tauri `app_config_dir`) — not a hidden JSON the user can't find; expose "Open data folder" in settings.
- First run < 2 seconds to usable; loading states, not spinners-on-white.
- Performance: measure RAM at idle (desktop apps live forever; 1 GB after a day is a bug), boot time, update time.

## Testing & CI

- Smoke: boot → key flow → quit, on all three OSes in CI (GitHub Actions macos-latest / windows-latest / ubuntu-latest; container for Linux GUI tests: Xvfb).
- UI E2E: Playwright against the webview (Electron: spectron-style or Playwright + bundled app; Tauri: webdriver for Wry or Playwright against the dev server).
- Pack on CI, not locally: the "works on my machine build" divergence is the #1 release bug. Upload artifacts; sign on CI machines (certs in CI secrets).

## Failure signatures → cause

- Crashes only after install (not dev) → asarUnpack/native addons, or missing code-sign on a dylib (macOS) → re-sign everything.
- "App was damaged / can't be opened" (macOS) → missing notarization or ad-hoc signature → notarize.
- SmartScreen "Windows protected your PC" → unsigned cert / low reputation → sign, distribute, build reputation.
- IPC calls work in dev, fail in prod → different CSP/capabilities/sandbox between dev (loose) and release build → run the release build in dev before shipping.
- Update loop (app re-downloads same version) → version compare wrong (string vs semver) or checksum always mismatched (payload re-encoded on your server) → pin the format, test the compare function.
- Memory climbs over days → orphaned listeners, leaked workers, unclosed DB connections (desktop = long-lived process; what's a 10 MB leak in a web tab is a 1 GB leak over a week of the app open).

## Detailed coverage

Cross-platform desktop application development — choosing Electron vs Tauri vs native, security model (context isolation, capabilities), OS integration (tray, shortcuts, deep links), packing & code signing (macOS notarization, Windows SmartScreen, Linux AppImage/flatpak), auto-updates, offline-first storage, and common failures. Use when building or debugging a desktop app.
