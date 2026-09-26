---
name: colors
description: Color systems for product UI — building palettes, contrast and WCAG accessibility, oklch ramps, dark mode, semantic tokens, and avoiding AI-slop palettes. Use when designing, auditing, or fixing colors, themes, or color-blindness/contrast issues.
---

# Color Systems

## Building a palette (in this order)

1. **Neutrals first.** One hue-tinted neutral ramp (10–13 steps) — pure gray feels dead for warm brands, too-tinted turns walls. Derive with oklch: fix hue/chroma (chroma ~0.005–0.01 for steps 50–950), vary lightness 98→10.
2. **One accent.** A single brand hue. Derive its ramp the same way; accent steps used on text need 4.5:1 against the surface.
3. **Semantic set.** success / warning / danger / info — each a hue distinct from brand, each with a ramp (solid, tinted background, text-on-tint). Pick tints by lightness so they read as "soft version of the state": e.g. `oklch(95% 0.03 145)` bg with `oklch(35% 0.12 145)` text.
4. **Surfaces & elevation.** Layer surfaces by lightness steps (or saturation in dark mode), not by blur/glass. Shadow: subtle, low-alpha, hue-matched (`oklch(from <brand> l c h / 8%)` or simple `rgba(0,0,0,0.06)`).

## Contrast (hard rules)

- Body text: **4.5:1** (AA) on its background; large text (≥18.66px bold / 24px): **3:1**.
- UI components & graphical objects (icons, borders, focus rings, input outlines): **3:1**.
- Placeholder text is not exempt — it carries meaning ("hint me what goes here").
- Check against the *actual rendered* background (cards, modals, overlays), not the page white. Compute with a real contrast tool, not by eye.
- Never color-only: pair state colors with icon/label/text (color-blind users + low-vision).
- Focus ring: 3:1 against adjacent colors, ≥2px, always visible.

## Tools

- Prefer **oklch** in CSS: perceptually even ramps, one-axis variation (`oklch(70% 0.15 250 / 0.9)`), color functions (`oklch(from var(--brand) calc(l - 20%) c h)`).
- `color-mix(in oklab, var(--accent) 10%, white)` for tinting without hand-computing.
- Define semantic tokens (`--surface`, `--on-surface`, `--accent`, `--danger-bg`…) and never sprinkle raw hex in components.

## Dark mode

- Don't invert. Desaturate accents (raise L to ~80–90%, drop chroma), lift true black to near-black (`oklch(15%)` background, `oklch(22%)` surface) to keep layering readable.
- Reduce shadow opacity; use surface lightness steps for elevation instead.
- Re-verify contrast per theme — a passing light-mode color often fails dark.

## AI-slop tells (avoid)

- Purple-blue gradient heroes, full-spectrum gradient text, rainbow dashboards with 8+ saturated hues.
- Pure `#fff` / `#000` as brand surfaces; single neon accent on black with no neutral system.
- Color used as decoration while the actual semantic state is unstyled.

## Audit checklist

[ ] Every text token ≥4.5:1 on real backgrounds · [ ] Icons/borders ≥3:1 · [ ] Focus visible ≥2px · [ ] State colors paired with non-color cue · [ ] No raw hex in components · [ ] Dark theme contrast re-checked · [ ] Color-blind simulation of the two busiest screens.