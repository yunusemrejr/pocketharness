---
name: winecharm
description: "Run and manage Windows apps on Linux with WineCharm: prefixes, templates, runners, headless .charm scripts, portable backups."
---

# WineCharm

Use for WineCharm work: installing it, managing prefixes/templates/runners,
launching `.exe`/`.msi`, automating via headless `.charm` scripts, and
portable backups. WineCharm manages Wine; hard app failures debug at the
Wine layer (see windows-on-linux-engineering).

## Working method

- Install via Flatpak (`io.github.fastrizwaan.WineCharm`) or pip with
  system Wine plus `wine32`/`wine64`, winetricks, icoutils, exiftool.
- One prefix per app from a matching `win32`/`win64` template; pin a
  runner per prefix (system Wine, wine-proton, wine-wow64, stable/devel).
- Clone before experimenting; export `.prefix`/`.bottle`/`.wzt` before
  destructive operations and verify restore on a copy.
- Keep `.charm` scripts in version control as the reproducible record.

Read [patterns and examples](references/patterns.md) for commands, data
layout, and troubleshooting order.

## Evidence and completion

Report install method, template, runner, and exact launch command. Verify
by launching the app to its working state.
