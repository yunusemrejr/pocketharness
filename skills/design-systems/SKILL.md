---
name: design-systems
description: Build and evolve design tokens, themes, component contracts and reusable UI systems. Use for consistent product styling and component architecture.
---


# Design Systems

## What a system actually is

A design system is **the single source of truth for visual + behavioral decisions**, so that "make it green" and "how do we style this dialog" have one answer each. It's not a component library wearing a costume. Three layers, in order:

1. **Tokens** — the raw decisions (values).
2. **Components** — composed from tokens (behavior + a11y + states baked in).
3. **Patterns/meta** — layout grids, type ramp, spacing rules, content rules, docs, governance.

If you ship "just components," you've built a kit, not a system — every consumer re-decides spacing/color, and drift is guaranteed.

## Token architecture (the part that earns its keep)

Three tiers, **never reference a lower tier from a component**:

```
Primitive         Semantic               Component
white-50      →   bg-surface        →   bg-card
gray-700      →   text-primary      →   title-heading
blue-600      →   accent-primary    →   action-button-bg
8px           →   space-4           →   input-padding
14px          →   text-body-size    →   button-font
```

- **Primitives**: raw scale (`white-50…950`, `space-0…16`, `radius-0…full`, `text-xs…5xl`). Named by scale position or neutral hue + step — **not by use** (`bg-gray-200` is a primitive, `bg-warning-subtle` is semantic).
- **Semantic**: the *meaning* layer — `bg-surface`, `bg-surface-raised`, `text-primary/secondary/muted`, `border-default/divider`, `accent-primary`, `state-error/warning/success`, `shadow-card/popover`. This is the theming seam: **dark mode = re-mapping sematics, nothing else.** A rebrand = swapping ~10 primitive values. If theming requires touching components, the tiers are wrong.
- **Component**: component-private values (`button-padding-x: 12px`) — rare, documented, and pointed at semantic where possible.
- Naming is the API: `color-bg-surface-1` > `surface1`? No — **readable and sortable in the IDE**: `<entity>-<role>-<qualifier>` (kebab, consistent prefix). Consistency matters more than elegance; document the rule in one place, enforce with lint (stylelint custom-property pattern).
- Output: JSON/TS source of truth → generated **CSS custom properties** (`:root`, `[data-theme=dark]`) + TS constants for canvas/SVG. Generate, don't hand-maintain both.

## Component API (where systems live or die)

- **Variants, not prop soup:** a button has `variant` (primary/secondary/ghost/danger), `size` (sm/md/lg — 3, not 5), `loading`, `disabled` — done. A 40-variant button is a failed abstraction; composition (icon slot, label, actions) covers the rest.
- **States are part of the contract, not an afterthought:** default / hover / focus-visible / active / disabled / loading / (error, for inputs) / (selected, for collections). Every component ships all applicable; a11y states (aria) baked in — a component that requires the consumer to remember `aria-*` is a trap. (Checklist from `ui-ux-principles`: the 5 product states, focus, contrast.)
- **Composition over configuration:** slots/slots (`<Dialog header footer actions>`), not `headerText`, `footerAlign`, `showSecondaryAction` booleans. Booleans = combinatorial explosion + a naming tax forever.
- **One primary interaction, and it's keyboard-complete:** tabs/arrow keys/escape where the pattern demands (menus, listboxes, dialogs). If a team can't ship the keyboard path, the component isn't done.
- **Uncontrolled *and* controlled** where users drive value (input, select, checkbox): `value` + `onChange` OR defaultValue; document which, provide the wrapper. Half-baked "controlled" (value without onChange silently no-ops) is a top-3 support-ticket generator.
- **Sizing scale:** S/M/L maps to type ramp + spacing scale (so "md button" and "md density" rhyme); density toggle = one variable, not 30 paddings.
- **Escape hatches exist and are narrow:** `className` passthrough for layout (positioning), never a `style={{...}}` that overrides the component's own semantics; "style override" escape hatches that let consumers kill the system's a11y = the system is a suggestion.

## Theming

- `[data-theme]` / `prefers-color-scheme` at the root swap semantic tokens; **components never hard-code a value** (enforce by lint: no raw hex/hsl in component styles).
- Dark mode is a *re-mapping*, including shadows (darker, tighter), borders (lightly brighter than surfaces — dark UIs need 1–2% lighter dividers or they vanish), and imagery (slightly desaturate / lower luminance).
- Brand themes (white-label): expose **semantic + a few primitives**, forbid consumers from touching component tokens; ship a "theme checker" (contrast + spacing audit) in CI for brand themes (WCAG: `colors` skill).
- Motion/elevation/typography are also themes (a dense admin theme and a marketing theme differ in those, not just in hues).

## Docs & adoption (the unglamorous 60%)

- **Live, copy-pasteable examples** per component (real usage, in your actual product) + **do/don't** pairs (the don'ts are what you learned from real misuse — keep updating them).
- One command to run a kitchen-sink storybook/kitchen-sink page; token reference page (auto-generated from source).
- **Migration path** for adopting: tokens first (CSS vars everywhere, delete the ad-hoc hexes), then components per page (not "all at once" — ship the system, migrate the hottest surfaces, lint the rest).
- **Versioning:** public packages with semver; a breaking change = migration guide + codemod where practical; deprecate (warn in console for a release) before removing.
- **Governance, in one paragraph:** who approves a new component (2 reviewers, one design), when it's added (**third real use, never a hypothetical** — first use is a local component; the *pattern* promotes), how changes land (API review before code), what's out of scope (anything done once). This is the ponytail rule at system scale: the system is the place where premature abstraction accumulates, so the gate is "already needed twice."

## Failure modes (diagnose before refactoring)

- **Component dump**: library with 40 parts, no tokens, no docs → product drifts immediately; fix = tokens layer + pick the 15 real components, park the rest.
- **Two tokens**: "the new way" and "legacy hex" coexist → add a codemod/lint rule, dead-line the legacy, then delete.
- **Over-varianting**: 9 button variants, 3 used → cut to the used set; composition handles the rest.
- **God-props**: a component with 14 optional props each "for that one case" → split (two smaller components) or slot it.
- **Style-fighting consumers** (every override exists to kill a default) → the default is wrong; change the default, delete the overrides, note in changelog.
- **System outpaces product** (200 components for a 10-screen app) → stop; the system is for the *next* product, not the current one; YAGNI applies to systems too.
- **Design and code diverge** (Figma file is the "source") → single source of truth decision (usually: code tokens, Figma pulls from them via a tokens pipeline); design owns *intent*, code owns *values*.

## Checklist before a component ships

[ ] All states incl. focus-visible + a11y roles · [ ] tokens only, no raw values · [ ] keyboard path works (Tab/arrows/escape) · [ ] reduced-motion variant · [ ] light + dark rendered · [ ] 360 px and 1440 px · [ ] docs: usage + do/don't + variant table · [ ] no escape hatch that breaks a11y · [ ] used in *this* product before it's "shipped."

## Detailed coverage

Building & using design systems — token architecture (primitive → semantic → component), naming that survives rebrands, theming (light/dark/brand) via CSS custom properties, component API design (states, variants, composition), versioning, docs, governance, and avoiding "component dump" failure modes. Use when creating, extending, or adopting a design system, or when UI styling has drifted out of control.
