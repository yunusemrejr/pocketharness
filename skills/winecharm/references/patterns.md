# WineCharm patterns and examples

Primary references: https://github.com/fastrizwaan/WineCharm (README,
install, usage) and https://flathub.org/apps/io.github.fastrizwaan.WineCharm
(Flatpak). Wine-layer debugging lives in windows-on-linux-engineering.

## Install

- Flatpak: `flatpak --user install flathub io.github.fastrizwaan.WineCharm`,
  run `flatpak run io.github.fastrizwaan.WineCharm`. Sandboxed with Wine
  stable bundled; broad access to `~/Games`, `.wine`, Bottles data.
- Pip/developer: needs `wine wine32 wine64 winetricks icoutils
  libimage-exiftool-perl zstd winbind python3-yaml python3-psutil
  libgtk-4-1 libadwaita-1-0 python3-gi` (Debian names) plus GTK4 4.16+
  and Libadwaita 1.7+. Verify with `winecharm --help`.
- GUI: `winecharm` with no arguments; Open picks `.exe`/`.msi`; advanced
  options in the hamburger menu and per-app settings page.

## Data layout

- Flatpak: `~/.var/app/io.github.fastrizwaan.WineCharm/data/winecharm/`;
  system installs keep data under the user's home. `Settings.yaml` holds
  settings. `Prefixes/` (per-app prefixes), `Templates/` (base prefixes
  `WineCharm-win32`/`WineCharm-win64`), `Runners/` (custom Wine builds).
- Backups: `.prefix` (prefix backup), `.bottle` (portable prefix with
  game and runner data), `.wzt` (WineZGUI-compatible).

## Headless automation

- `winecharm /path/to/script.charm` runs a script;
  `winecharm /path/to/file.exe` launches directly;
  `winecharm /path/to/backup.wzt` restores a backup.
- Non-interactive runs need the same prefix/runner data present;
  WineCharm does not fetch missing runners by itself.

## Troubleshooting order

1. Classify: installer vs first-run vs runtime failure; 32- vs 64-bit.
2. Check prefix/runner pairing first: 32-bit installers on win64
   prefixes and wrong-runner launches beat reinstalling as a cause.
3. Diff `Settings.yaml` and per-app settings against a known-good
   template before touching global settings.
4. Escalate missing-DLL/API-stub/graphics failures to Wine debugging
   (`WINEDEBUG`, `WINEDLLOVERRIDES`, winetricks verbs).
5. Never delete the last good prefix backup until its replacement is
   verified working.
