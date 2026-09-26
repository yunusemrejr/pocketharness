# Stacks and toolkits

## Native toolkits

GTK (with libadwaita for GNOME-style apps) and Qt/QML are the native choices. GTK fits GNOME conventions (header bars, adaptive layouts) and packages naturally on GNOME distributions; Qt/QML fits KDE conventions and offers one codebase across desktop and embedded with strong QML styling. Both have mature accessibility bridges and Wayland support — verify the versions your minimum distribution ships, since toolkit behavior (file choosers, portals, fractional scaling) changes across releases.

Glade `.ui` files and Qt Designer forms keep layouts declarative and reviewable; QML goes further with reactive bindings. Prefer declarative UI over imperative widget code for anything a designer must reason about. Keep business logic out of UI files so the interface can be restyled without touching behavior.

## Web-technology runtimes

Electron ships a full Chromium runtime: maximum web-stack reuse at the cost of large downloads, heavy memory use, and responsibility for Chromium security updates. Tauri uses the system webview with a Rust backend: far smaller bundles and lower memory, with webview-version variance across distributions as the tradeoff. Choose Electron when you need exact Chromium behavior everywhere; choose Tauri when bundle size and resource use matter and you can test across webview versions.

Both must handle Linux specifics: transparent windows and decorations under Wayland versus X11, global menus, tray icons (StatusNotifier versus legacy), and file dialogs through portals when sandboxed. Test windowing behavior on both display servers — web code cannot see these failures.

## Flutter and other cross-platform UI

Flutter draws its own widgets: consistent rendering everywhere, at the cost of non-native feel, larger binaries, and an accessibility story that must be verified per release on Linux. It suits branded, custom-designed apps more than utilities expected to blend into GNOME or KDE. Whatever the framework, verify text input (IME, dead keys, compose), clipboard formats, and drag-and-drop against real desktop sessions — self-drawn UI breaks these silently.

## Choosing: a decision record

Score each candidate on: native integration weight, bundle size, startup time, accessibility maturity, theming flexibility, team skill, and packaging friction for your targets (native packages, Flatpak, AppImage). Write the scores and the rejected alternatives down; revisit when a constraint changes rather than drifting stacks mid-project. A prototype that renders one dialog in each finalist toolkit, on the target desktops, is worth more than a week of document comparison.

## Desktop conventions that differ

- GNOME favors header bars, minimal chrome, and adaptive narrow layouts; KDE favors traditional menu bars, rich configuration, and powerful defaults. Respect the host environment's patterns instead of imposing one desktop's idioms everywhere.
- Window controls, focus behavior, and workspace semantics belong to the window manager — do not fight them. Custom title bars must still support move, resize, snap, and accessibility actions.
- Icon themes, cursor themes, and font settings come from the user. Ship symbolic-style icons that recolor, and never hardcode system font names.

## Primary references

Check the documentation for the deployed version when an API or behavior matters. These are reference entry points, not permission to install or deploy.

- https://docs.gtk.org/
- https://doc.qt.io/
- https://www.electronjs.org/docs/latest/
- https://tauri.app/
- https://docs.flutter.dev/platform-integration/linux/
