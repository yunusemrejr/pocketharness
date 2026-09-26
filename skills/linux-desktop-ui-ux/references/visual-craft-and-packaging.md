# Visual craft and packaging

## Theming

Support both light and dark themes from the start, following the system preference via the desktop portal or toolkit settings — never a hardcoded default. Accent colors, selection colors, and destructive-action colors should derive from the theme where the toolkit allows; custom brand colors must still meet contrast in both themes. Test theme switching at runtime: a restart-to-retheme app fails the basic desktop expectation.

Hardcoded colors are the most common Linux theming bug: dark text on a theme-provided dark background, invisible selection, unreadable disabled states. Audit every custom color against both themes and against high-contrast modes. Respect the user's reduced-motion and reduced-transparency signals where the platform exposes them.

## Typography and HiDPI

Use the system font stack by default so the app matches user configuration and language coverage; custom fonts are a branding decision with licensing, loading, and fallback costs. Verify hinting and anti-aliasing on low-DPI screens and fractional scaling (125%, 150%) on HiDPI — text that is crisp at 100% and 200% can still smear at 150% depending on the toolkit path.

Design layouts that survive font-size changes and translation growth (German and Finnish strings run long). Fixed-pixel layouts clip; constraint or flow layouts adapt. Verify at 200% scaling and with large-text accessibility settings: dialogs must scroll or grow rather than hiding their action buttons.

## Density and desktop ergonomics

Desktop apps show more at once than mobile: data tables, sidebars, toolbars, status bars, and multi-pane layouts. Use progressive disclosure (collapsible sections, advanced tabs) instead of removing power features. Keep destructive and irreversible actions explicit with undo where feasible; confirmation dialogs are a last resort, not an interaction model.

Keyboard operation is non-negotiable: full tab order, mnemonics or accelerators for primary actions, Escape behavior per dialog, and discoverable shortcuts (a shortcuts window beats a wiki page). Verify with keyboard only, then with a screen reader over the accessibility bridge.

## Packaging UX

Flatpak, AppImage, Snap, and native packages each change the user experience: sandbox permissions (file, camera, network portals), first-run setup, update prompts, and MIME/file-association registration. Declare minimal permissions and request the rest through portals at point of use; an app that demands full filesystem access for one export dialog erodes trust.

Write the MetaInfo/AppStream data carefully: name, summary, description, screenshots at real window sizes, release notes, and content rating. This metadata is the store listing on Flathub and in software centers — it deserves the same design review as the app. Screenshots must show the actual current UI in the default theme, not aspirational mockups.

Credentials (API keys, account passwords, license tokens) live in the OS keyring or the existing secret mechanism — never in config files in plain text, logs, or screenshots. Review every screenshot and screen recording for leaked tokens, personal paths, and private data before publishing.

## Primary references

- https://developer.gnome.org/hig/
- https://develop.kde.org/hig/
- https://docs.flathub.org/docs/for-app-authors/metainfo-guidelines/
- https://www.freedesktop.org/wiki/Specifications/
