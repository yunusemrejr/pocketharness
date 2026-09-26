---
name: desktop-ui
description: 'Design desktop interaction for Linux, macOS and Windows: windows, menus, shortcuts, trays, dialogs and dense controls. Use for native-feeling desktop UI.'
---


# Desktop UI/UX conventions

Desktop users run these for hours, on large screens, keyboard-primary. Three universal rules: **every action has a keyboard path**, **windows remember themselves** (size/position/maximized, tab state), and **the OS owns the chrome** (title bar, menus, file dialogs) unless you have a strong reason (and you usually don't).

## Universal conventions (all platforms)

- **Menus & accelerators**: full app menu (File/Edit/View/Help minimum; macOS: app menu first with About/Quit). Accelerators visible in the menu (`New  Ctrl+N` / `⌘N`) — hidden shortcuts are not discoverable and the menu is the discoverability surface. Standard families exist — use them, never invent: ⌘N/⌘W/⌘Q, ⌘S, ⌘Z/⌘⇧Z (undo/redo), ⌘A, ⌘C/X/V, ⌘F (find), ⌘, (settings/preferences — **macOS only**; ⌘, is not the convention on Win/Linux where Settings is in the menu or a gear), ⌘P print.
- **Context menus**: right-click (long-press on touch) brings the object's actions, most-used first, destructive last + separator, **enabled/disabled states visible** (grayed, not removed), a "details/properties" at the bottom. Left-click does the default; double-click activates (open/expand).
- **File dialogs**: **always the native open/save dialog** — never a custom in-app file browser (macOS sandbox *requires* it; users also just expect it). Templates/recents in the dialog itself are fine; a parallel in-app dialog is not.
- **Drag & drop is a first-class citizen**: drag files in (the drop target = the window or an explicit zone, with hover feedback), drag out to export/share where it makes sense, drag-to-reorder in lists (with a gap indicator), copy-selection → paste anywhere. DnD is *cheaper than a button* for power users — if a flow needs a button to move data between two of your own panels, redesign it as a drag.
- **Multi-select & bulk bars**: shift-click/ctrl-click to select, a bulk action bar appears (count + actions: "3 selected → Delete, Tag, Move"), the selection survives navigation where it makes sense. Desktop is where multi-select lives — a web app that only single-selects on a 1400px screen is leaving the room.
- **Keyboard-first layouts**: everything reachable without a mouse (tab order logical, ⌘1-9 for views, ⌘K command palette is the modern power-user layer); mouse-only controls are a defect, not a shortcut.
- **Density controls**: compact/comfortable/relaxed toggle for lists and tables (users with density needs *ask for it*); comfortable is the default, compact exists.
- **Status & toasts**: transient confirmations are **toasts at the bottom** (4-8s, stack ≤ 3) with an action button (Undo) where applicable; persistent state (sync, connection) is a **status region** (menubar extra / tray / in-window indicator), never a toast. **Undo beats confirm** for reversible deletions; the irreversible gets a named confirm dialog (type-to-confirm only for the org/workspace nukes).
- **Undo on local work**: local operations (edits, reorders, deletions of local items) undo for at least a session (a stack, not a one-shot toast); cloud-synced deletions get a Trash + a window (30d standard).
- **Settings location**: one Settings/Preferences window (⌘, on macOS), organized in a left-nav groups (General, Appearance, Shortcuts, Notifications, Data, Advanced, Account), search inside it (⌘K in settings = the modern pattern), the dangerous actions in a **danger zone** at the bottom (red-tinted, named confirms). Per-item settings live near the item (the gear on the card), global in the window.
- **Trust OS dark mode**: follow it (no in-app override that fights the system — an override exists, but the default = follow).

## Windows (Fluent 2)

- **Title bar**: app icon + title left, min/maximize/close right (close = the only one that gets a hover-red). Custom title bars (Electron default) must put the caption controls in the right corner, be a real clickable drag region, and survive **125/150% scaling** — the classic breakage is controls clipped at high DPI or overlapping custom content.
- **Design language**: Segoe UI Variable (14px base, 12px secondary), an 8pt spacing grid, 4-8px corner radius, **Mica** (window backdrop = desktop-tinted) / **Acrylic** (sub-layer blur — sparingly), elevation via layering+shadow (4 levels, don't stack), a single accent color (the user's *system accent* is a legitimate choice — theme to it), light/dark via system.
- **Menus**: the app menu is still expected (Win users know File/Edit). A web-style top nav bar without a menu = "web app" tell. Toolbars may carry frequent commands (icon + accelerator tooltip).
- **Tray/status**: tray icon = background-presence (right-click → context menu with Open/Quit; **a tray icon you can't quit from is a finding**). Notifications → the Action Center (toast, actionable, ≤ 4 actions).
- **Shortcuts**: Win+letter for app-wide where you support global, Alt for menu activation (the Alt-menus still exist — keep them), **⌘K → Ctrl+K** equivalence for palettes.
- Test at 100/125/150% scaling and with "Let apps decide / Let Windows decide" both (per-monitor DPI is where custom title bars die).

## macOS (HIG)

- **Traffic lights, top-left, never moved** — hidden-titlebar designs inset content past them (safe-area); a custom title bar that *covers* them or moves them is the instant "this isn't a Mac app" tell. Close/minimize/zoom left-to-right in that exact meaning.
- **Menu bar is the app's**: app menu first (`AppName`: About, Preferences ⌘,, Services, Hide, Quit ⌘Q), then File/Edit/View/Window/Help — a menu bar that looks like a generic web nav (no app-named first menu) = web costume. **⌘Q quits; closing the window is not quitting** (document apps reopen the last window on relaunch; single-window utilities may quit-on-close, but know which you are and be consistent).
- **Design language**: San Francisco (13px base, 12px secondary), 8pt grid, restrained corner radius, vibrancy only for chrome (never body surfaces), springs for motion (macOS motion is *springy*, not linear), a single accent (system accent honored).
- **Preferences (⌘,) tabbed window** is the classic; grouped single-window is fine — the point is one place. **Status items** (menu bar extras): simple, icon-only preferred, a context menu, no "notification center" games (that's the OS's).
- **Windowing**: resizable documents win; minimize → Stage Manager/Exposé must not break (test the minimize). Tabs: macOS-native tabbing is expected in document apps (or explicitly support it); a web-style in-app tab bar that ignores OS tabbing is a mixed signal (pick one).
- **Files**: sandboxing — user files only via the open/save panels (security-scoped bookmarks to persist); app-specific data in `~/Library/Application Support/<id>`; no writing to arbitrary paths "because the user said so" without the panel.
- Test on light+dark (macOS dark is *the* environment for a dark-first product), Stage Manager, and an external 4K (the scaling modes).

## Linux (GNOME + KDE reality)

There is no one desktop: **GNOME** (Adwaita, flat, header bars *instead of* title bars, window controls in the top panel *not* on the window, the app menu in the header bar, simple/limited) and **KDE** (Breeze, traditional title bars with caption buttons, classic menus, more chrome, more options) — plus Xfce for thinner/older, and Wayland vs X11 differences (global shortcuts are restricted on many Wayland compositors — **don't build product flows on custom global hotkeys** on Linux).
- Adhere to the **freedesktop specs**: `.desktop` files, icon themes (Adwaita/Breeze/elementary — ship an icon in the theme format, don't inline your own icon set in context menus where the platform provides them), the StatusNotifier (tray) for the background icon, the portal APIs (file dialogs, notifications) via the toolkit — Qt6/GTK4 + libadwaita give you platform-correct behavior for free; a GTK/Electron app that draws its own title bar on Linux is the *most* visible web-costume tell (GNOME's top panel hosts the caption buttons — your fake title bar collides with the real one).
- Dark mode: `gtk-application-prefer-dark-theme` / KDE palette — follow the system; "force both" (a separate in-app theme control) is a fallback last layer, not the default.
- Fonts: 11pt sans (Cantarell/Ubuntu/Noto), the 8pt grid still works; test on GNOME (Wayland), KDE (Wayland + X11) minimum. If the product targets "Linux" at all, **KDE + X11 is where the power users live and the assumptions break** (global shortcuts, secondary-screen scaling).
- Don't assume a system tray exists (GNOME has no default tray — the StatusNotifier needs an extension on some setups; provide an in-app background-state indicator too).

## Hybrid apps (Electron/Tauri) — the "web costume" checklist (the audit)

The failure mode is a webview wearing a desktop costume; pass this before calling it a real app:
1. **Full app menu with accelerators** (the #1 missing piece — a top nav bar instead of File/Edit = instant tell; Electron: build the menu, Tauri: menu API).
2. **Native title bar or a correctly-inset one**: traffic lights safe-area on macOS, caption controls right + DPI-safe on Windows, *no window chrome* on Linux (let the compositor own it).
3. **Native dialogs**: open/save/file-export = OS dialogs (both platforms); native notifications; native context menus (right-click = OS-styled, not a div).
4. **Tray/status item** with Open + Quit in its menu; the app starts minimized-to-tray when the user expects a background app (setting, not default-surprise).
5. **Window state persistence** (size/position/maximized, remembered per-window) + single-instance (second launch focuses the first).
6. **OS-integrated basics**: dark-mode follow, ⌘/Ctrl accelerators incl. ⌘, and ⌘Q/**Ctrl+Q** (Windows: close-all semantics ≠ quit — know it), global shortcuts (where they work — Linux/X11 caveat), file associations + "open with" menu.
7. **DnD**: files dragged from the OS into the window work (drop zone), dragged out where reasonable.
8. **Offline/decoupled**: the app opens without the network and says so (status), syncs when back (a visible queue: "3 changes will sync"), local undo is instant (never "saving… 4s" locally).
9. **DPI**: 125/150% on Windows, Retina + external 4K on macOS — no clipped caption controls, no 1px hairlines that vanish, no text that gets bold/different-weight at non-integer scales.
10. **The keyboard**: every primary action on a chord; ⌘K/Ctrl+K palette for the power layer; tab order sane; the address bar of the webview is not a control (no visible devtools artifacts).

## Dense-data patterns (where desktop earns its keep)

- **Tables**: sortable headers (arrow on active), a per-column filter row (or a filter bar), pagination with "1–20 of 1,204" (or virtualize > ~100 visible rows), bulk bar on selection, row action ≤ 3 inline + ⋯ overflow, **detail in a side drawer (480-560px)** not a new window/page (context is preserved; ⌘W on the drawer closes the drawer, not the app-window).
- **Multi-document**: tabs within a window (the current object's views) vs a window per document (the macOS-native expectation) — pick by model: browser-style (many, cheap) → tabs; document-style (few, heavy) → windows. Breadcrumbs for depth > 3.
- **Minimap/outline** (code, docs, long lists): a left gutter overview + jump (⌘L in many editors; the palette covers it too).
- **Command palette (⌘K/Ctrl+K)**: search + actions ("Go to …", "Create …", "Toggle …") with a recent/last-used ordering — the modern replacement for *remembering* shortcuts; it doesn't replace the menu, it complements it.
- **Shortcuts cheat sheet**: in-app (Help → Shortcuts or `?`) — a list is expected in anything power-user; ship it.

## Failure tells (quick audit)

Web nav bar where a menu should be · no accelerators (or accelerators that differ from the platform families) · custom title bar with broken/misplaced caption controls · modals-that-open-modals (flatten; one dialog at a time, it's 2006 otherwise) · a "spinner in the middle of everything" instead of inline state · web-only toasts for *persistent* state · 100% DPI-only design (clips at 150%) · mouse-only flows · files exported via a web `<a download>` that fails on the desktop · tray icon without a Quit (the forever-process finding) · forced light mode on a dark OS (or vice versa) · a settings screen that's a website, not a window · no undo on local delete · "click here" affordance density (desktop users look for verbs in menus — give them there).

## Detailed coverage

Desktop GUI/UX conventions for Windows, macOS and Linux (Fluent 2, Apple HIG, GNOME/KDE HIG) — menus, accelerators, title bars & window chrome, tray/status items, dialogs & context menus, settings placement, DPI scaling, keyboard-first design, native-feel for Electron/Tauri apps, dense-data patterns (tables, bulk actions, undo), and the tells of "web app wearing a desktop costume". Use when designing or reviewing any desktop GUI (including hybrid Electron/Tauri apps).
