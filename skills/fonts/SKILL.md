---
name: fonts
description: Typography for web and product UI — font selection, pairing, web font loading, variable fonts, fluid type scales, and text rendering quality. Use when choosing or implementing typefaces, fixing text rendering, fixing layout shift from fonts, or building a type scale.
---

# Fonts & Typography

## Pairing discipline

- Max **two families**: one for display (headlines), one for text (body). Add a monospace only when showing code. Never a third "decorative" face.
- Contrast the pair: different weight, form, or category (e.g. grotesque body + humanist serif display). Two similar sans-serifs = mush.
- Body faces must be designed for small sizes and long reading: Inter, Public Sans, IBM Plex Sans, Source Sans 3, Söhne-class. Display faces can be expressive (Fraunces, Space Grotesk, Clash Display) but stay readable at 24px+.
- Check the pair at real content lengths, not lorem in a hero.

## Sizing & scale

- Fluid scale with `clamp()`: e.g. `--text-body: clamp(1rem, 0.95rem + 0.25vw, 1.125rem)`. Use a modular ratio (1.2/1.25/1.333) for step sizes.
- Body: 16px minimum, line-height 1.5–1.6. Large display type: line-height 1.0–1.2, tighten with `letter-spacing: -0.02em`.
- Line length 45–75 characters: `max-width: 65ch` on prose containers.
- Set sizes in `rem` (respect user zoom); never px on interactive text.

## Loading (the part that breaks sites)

- Self-host or use fontsource; skip generic Google Fonts `<link>` unless speed doesn't matter.
- Serve **woff2 only**, subset per locale (pyftsubset / fontsource per-cyrillic etc.). A 200KB font for a 5-letter logo is waste.
- `font-display: swap` for body text (FOUT beats invisible text); `optional` only when an immediate fallback is metric-compatible (use `size-adjust`, `ascent-override` on the fallback stack to avoid layout jump).
- Preload the primary font file only: `<link rel="preload" href="/Inter.woff2" as="font" type="font/woff2" crossorigin>` + `preconnect` to the font host.
- Variable fonts: load ONE file covering the weight axis instead of 4 static weights; use `font-variation-settings` or the `wght` axis via `font-weight` range.
- Zero shift: reserve space (fallback with size-adjust), or render text after `document.fonts.ready` if the jump is unavoidable.
- System-stack escape hatch when bytes budget is hard: `system-ui, -apple-system, "Segoe UI", Roboto` — no download, no CLS, perfectly acceptable for internal tools.

## Rendering details

- `font-feature-settings` / `font-variant-numeric`: `tnum` (tabular figures) for tables and dashboards, `lnum` for mixed prose + numbers.
- `text-wrap: balance` on headlines (equal line lengths), `text-wrap: pretty` on paragraphs (no orphans).
- `font-optical-sizing: auto` when the face has an optical size axis.
- Anti-fringe on small light text on dark: avoid `antialiased` aliasing tricks; fix with ≥15px + 4.5:1 contrast instead.

## Pitfalls

- Loading 5 weights × 2 families × 3 subsets = 30 requests. Ship what you use; grep `font-weight` values in CSS against loaded weights.
- Unoptimized SVG/PNG font previews, unminified CSS with `@font-face` far down the file (move to top).
- Using `px` line-height only — always pair with unitless line-height on responsive text.