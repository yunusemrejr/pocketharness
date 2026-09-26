---
name: linux-desktop-ui-ux
description: "Design advanced Linux desktop interfaces across GTK, Qt/QML, Electron, Tauri, and Flutter: toolkit choice, GNOME/KDE conventions, theming, typography, HiDPI, accessibility, Flatpak/AppImage packaging UX, and store presentation."
---

# Linux Desktop UI/UX

Use when designing or refining a visual desktop application on Linux — window and dialog behavior, toolkit choice, theming, density, packaging UX, or store listing. For runtime selection, OS integration mechanics, and update plumbing use desktop-app-dev; for interaction patterns (menus, shortcuts, trays, dialogs) use desktop-ui; for general design principles use ui-ux-principles; for verifying a built product UI use product-ui-verification.

## Working method

- Start from the desktop contract: which environments matter (GNOME, KDE, others), which display servers (Wayland, X11), and which distribution range. A design that assumes one theme, one scaling factor, or one window manager will break on real Linux desktops.
- Choose the toolkit against constraints, not fashion: native look and packaging weight, startup cost, accessibility bridge maturity, and the team's ability to theme and debug it. Record why the alternatives lost.
- Design the full window lifecycle: first launch, empty states, window restore, multi-window behavior, tray presence, notifications, and dark/light theme switching. Desktop apps live for hours; web-page thinking misses these states.
- Verify on real desktops: render under at least one GTK-based and one Qt-based environment, at 100% and 200% scaling, in both themes, with keyboard only. Screenshots from one machine are not Linux verification.

Read [stacks and toolkits](references/stacks-and-toolkits.md) when choosing or comparing implementation stacks. Read [visual craft and packaging](references/visual-craft-and-packaging.md) for theming, typography, accessibility, and distribution UX; do not load it for pure toolkit-spike work. User instructions take precedence; this skill adds no authority to change system themes or publish packages.

## Evidence and completion

Report the target environments, the toolkit decision with rationale, and what was actually rendered and where. Name untested combinations (desktop, scaling, theme) explicitly. Do not present a single-screenshot review as desktop verification.
