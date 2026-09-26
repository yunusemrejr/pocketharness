---
name: svg-assessment
description: >-
  How to assess, audit, and judge SVG designs and files — the quality checklist (viewBox, scaling, currentColor vs hardcoded fills, paths vs primitives, file size & node count, accessibility: title/desc/role/aria, gradients/masks/filters correctness and cost), the icon-set consistency audit (grid, stroke weight, optical size, "does it belong?" test), inline vs <img> vs <symbol>/sprite vs CSS-mask loading decision, optimization (SVGO: what to keep, what it breaks), security (scripts, external refs, nested SVG exposure), and a scoring rubric you can apply to "is this SVG good?". Use when reviewing an SVG (icon, illustration, logo, chart) before shipping, deciding how to load it, or when an icon set "doesn't feel consistent" and you need the names for why.
---

# SVG assessment (judging vector design & files)

`custom-svg` is how to *make* SVG; this is how to *judge* it. An assessment has two objects: the **design** (does it read, is it consistent, is it the right weight for its slot?) and the **file** (does it scale, style, load, and behave?). Most bad SVGs fail one side *because of* the other — the 400-node "simple" icon that is heavy *and* wrong-weight. This skill, `custom-svg`, `design-systems`, and the W3C SVG 2 spec (for the a11y attributes) are the references.

## The file side (the technical audit — the checkable list)

### 1. The viewBox & scaling (the #1 file bug)

- `viewBox` must be present and correct (an icon: `viewBox="0 0 24 24"` — the *coordinate space*, not the size). **An SVG with width/height and no viewBox does not scale** — it renders at fixed px. The "icon looks tiny" bug is 90% the missing viewBox.
- Root `width`/`height` are the *default* size (keep for the no-CSS fallback); real sizing is CSS. UI icons size in **`em`** (`width: 1.5em`) so they scale with the type; a `px` icon that outgrows its text is the mismatch tell.
- `preserveAspectRatio`: the default `xMidYMid meet` is almost always right. `slice` = cover (crops to fill — background illustrations), `none` = stretch (the "icon looks squashed" bug when the container's aspect ≠ the viewBox's aspect).
- The art must sit inside the viewBox **with padding** (24-grid convention: ~20px live area, 2px safe margin). `overflow: visible` as a "fix" is a *symptom* — redraw to the grid.

### 2. Fill: `currentColor` vs hardcoded (the #1 styling bug)

- Any icon that recolors with UI context must use `fill="currentColor"` (or `stroke="currentColor"`). `currentColor` inherits CSS `color`, so one SVG re-themes for free — dark mode, accent, hover all work without touching the asset.
- **Audit: grep the file for `fill="#…"` / `stroke="#…"`.** Hardcoded color in a *functional* icon = a finding (the black-icon-on-dark-bg is the most common production SVG failure, and it's one attribute). Exception: **branded** multi-color icons — the logo's colors are the brand; hardcoded is *correct* there.
- The test: drop it into a context where `color` is inverted — does it still read?

### 3. Geometry: primitives beat traced paths (the weight tell)

- `<circle>`/`<rect>`/`<line>`/`<ellipse>` beat a hand-traced `<path>` for the same shape on every axis: smaller (`<circle cx r>` ≈ 30 chars, the traced circle ≈ 120), exact (parametric vs bezier-approximation fuzz), and editable (moving a `cx` beats re-solving beziers).
- A **circle-as-path** (a 4-bezier "circle") is the tell of a tool that exports heavy — the "convert to outlines" / "flatten" step in the design file lost the primitives. Flag it; re-export *without* outlining, or redraw.
- **Node count**: a simple glyph (trash, bell, search) should be < ~20 path commands / < 1–2KB. A >5KB "simple" icon = node bloat (over-traced, over-beziered, unwelded). Open it in a text editor and count `d=""` segments — visual complexity and node complexity are *different things* (an ornate icon can be light; a simple one can be heavy — the latter is the tool-bloat case).

### 4. Gradients, masks, filters (correctness *and* cost)

- **Gradients**: fine and common (branded/soft icons). Judge the *stops* (2–3 = clean; a 7-stop "smooth" gradient = over-designed) and the *units* — prefer `objectBoundingBox` (portable, 0–1 space) over `userSpaceOnUse` (absolute coords; the "gradient shifts when the icon resizes" bug is `userSpaceOnUse` on a resized icon).
- **Masks/clips**: powerful and heavier (raster-composite cost per frame if animated). First ask: could `fill-rule="evenodd"` (the *free* hole) or a container `clip-path` do the job? The legitimate use is the *soft* mask (feathered edges, fades).
- **Filters**: `<feGaussianBlur>`, `<feDropShadow>` are the **expensive** end of SVG — per-frame raster work, and the main cause of "SVG that janks when it animates". A drop shadow that CSS `filter: drop-shadow()` would provide belongs in CSS; a blur that `backdrop`/stacking does, belongs outside the file. In-file filters must be *intrinsic to the art* (a lens flare's glow), and get a cost check before animation.

### 5. Accessibility (the attribute list)

- Decorative SVG: `aria-hidden="true"` (+ `focusable="false"` on older Edge/IE-era targets) — it must not pollute the screen-reader tree or the tab sequence.
- Informative SVG (a chart, an informative illustration): `role="img"` + `<title>` (+ `<desc>` for the long form) with `aria-labelledby` pointing at them; the *meaning* must survive without the pixels (a chart needs its numbers as text — pairs with `data-viz`'s "the number is text" rule).
- Interactive SVG (custom chart, canvas-like widget): `tabindex` + `role="button"`/`role="application"` *sparingly* — if you're building a rich interactive graphic, ask whether an HTML control behind it is simpler (usually is).
- **Text is text**: never paths-for-letters in a UI SVG that needs to be read (unselectable, unsearchable, the a11y hole, and the file weight). Paths-for-letters is the *logo-lockup* exception only.

### 6. Sharpness & HiDPI

- 1px strokes on even coordinates blur on non-retina screens — the half-pixel convention: a 1px vertical line at `x="0.5"` (or design at 2× and let it downscale). For icons, a **consistent stroke width** (the set's spec — 1.5 or 2 on a 24-grid) is the sharpness rule; a mixed-width set is the "inconsistent" tell before anything else is noticed.
- `shape-rendering="crispEdges"` only for hard geometric UI (grid lines) — it kills the anti-aliasing on everything else (never on curves).

## The design side (the judgment audit)

### The icon-set consistency audit (when a set "doesn't feel consistent")

- **The grid**: every icon on the same grid (24 or 32), same live area, same padding. One icon drawn at 22px live area in a 20px set reads as "too big" even when both are 24.
- **The stroke**: one width for the set (the dominant one — count them, 2.5 of 30 icons at 1.5 vs 5 at 2 = the spec was 1.5 and 5 are outliers); same **cap and join** (round-vs-square endings are the most visible inconsistency — a square-cap icon in a round set reads as a different *brand*).
- **Optical weight**: filled icons and stroked icons have to be *balanced*, not equal — a fill that's the same thickness as the neighbor's stroke sits visually *heavier* (the fill gets ~a half-step *less* visual presence: slightly smaller or the stroke slightly bolder). Mixed fill+stroke sets without optical correction = the "one icon is bold" tell.
- **Optical size**: fine details vanish below ~16px; icon sets ship 16/24/32 *variants* (the Lucide/Phosphor/Tabler model) — an asset set with only one size for all slots = "why is the toolbar icon mushy".
- **The feature consistency**: every icon's "extra" (the notch, the badge dot, the corner fold) at the *same corner* and same proportion — the badge dot at 3 o'clock in one icon and 1 o'clock in the other is the quiet inconsistency.
- **The "belongs" test**: place the icon next to the set's three most common icons (often: the home, the person, the settings) in one row at 24px. Whichever one *breaks the row's rhythm* is the outlier — this is the fastest test for "is this icon from the same family?", and it catches off-set icons (the one downloaded from a different pack) instantly.
- **The metaphor read**: at 16px, in the *actual UI* (not the icon grid), does the meaning land without a label? The icon that needs its tooltip to be understood is failing at the *job* — either redraw plainer or let the label do the work (label + icon is the honest combo for ambiguous glyphs).

### The scoring rubric (the "is this SVG good" 10-point)

1. viewBox present + em-sizing works (1)
2. currentColor where it should recolor (1)
3. primitives where possible, node count in range (1)
4. no external filters/masks without justification (1)
5. a11y correct for its role (decorative hidden / informative titled) (1)
6. text as text (1)
7. sharp at 16/24/32 in the real slot (1)
8. belongs in the set (the row test) (1)
9. file size in range for its class (1)
10. loads the right way for its use (`inline vs img vs sprite`, below) (1)

**8+**: ship. **6–7**: fix the two worst findings, re-check. **<6**: it's a re-extract or a redraw, not a debug.

## The loading decision (the file is fine — how does it get in?)

| Method | Styled per-context? | Animatable parts? | HTTP cost | Use when |
| --- | --- | --- | --- | --- |
| **Inline `<svg>`** in HTML/JSX | yes (`currentColor`, CSS on parts) | yes | 0 extra requests | UI icons, interactive/animated vectors, <~2KB |
| **`<img src=*.svg>`** | no (frozen document — CSS can't reach in) | no (except SMIL, deprecated) | 1 (cacheable) | *Inert* art that doesn't recolor (logo stamps, decorative art), the >5KB illustration |
| **`<symbol>` + sprite** (`<use href="#id">`) | root color yes (`currentColor`), parts no | limited | 1 sprite for N icons | large icon *sets* in non-build pipelines (the classic multi-page site icon system) |
| **CSS `mask` (mask-image: url)** | the *color* is the mask, style the bg | no | 1 | the pure recoloring case, `content`-style icons |
| **Font (icon font)** | yes | no | 1 font + the FOUT/CLS risk | **basically never in 2025** — the a11y, the ligature, the perf, and the tree-shaking problems all have better answers; legacy only |

The rule of thumb: **inline for the UI, `<img>` for the inert art, sprite for the big-inert-set**, and the *one* time to overthink: the SVG that must recolor *and* cross a build boundary (the mask technique, or the build-time inlining — `vite-plugin-svgr`-class, the `?react`/`?component` imports).

## Optimization (SVGO & friends)

- Run `svgo` (`--multipass`, `--precision=1`) in the build, not by hand — but **read the diff**:
  - **Keep**: `viewBox`, `currentColor`, `role`/`aria-*`/`<title>` (a11y), and any `id` that is *referenced* (`url(#grad)`, `<use href>`).
  - **What it breaks**: it *renames* `id`s (fine locally, breaks when you inline the same file twice in one document — the **duplicate-`id` collision** is the "second icon loses its gradient" bug; the fix: unique-ify per instance, or the `use`/`symbol` pattern, or the per-file CSS-scoped `id`s), it can *strip* `class` names you style, and it may drop the `overflow`/`preserveAspectRatio` you needed.
  - The `--disable` flags for the destructive passes (`convertPathData`, `removeViewBox` — never let it remove the viewBox).
- **The size classes** (the ranges the rubric uses): UI glyph <1–2KB; detailed icon <5KB; inline illustration <15–25KB; beyond that → it's an *image*, serve it as a file (AVIF-rendered if it's photographic-ish — `media-in-web`), or split it.

## Security (the exposure side)

- **Inline SVG is code surface**: `<script>` (rare, but in a `filter`/`foreignObject`/direct-inject path it executes), `onload`/event-handler attributes, `<a xlink:href="javascript:…">`, `data:` / external `href` fetches. **Any SVG from a user or a feed is a payload** — sanitize (DOMPurify with an SVG profile, or render via `<img>`/`mask` where the document is *frozen* — the `<img>`-loaded SVG is sandboxed from the page's DOM/JS: that's the **safe** way to display untrusted vector art).
- `foreignObject` (HTML inside SVG): the iframe-ish hole — scripts inside *can* run in most embedding contexts; treat any `foreignObject` you didn't write as hostile.
- **The `object`/`embed` of an SVG**: full document context — same trust level as an iframe of arbitrary content.
- **`xlink:href` / `href` to external URLs**: an `<use href="remote.svg#id">` fetches and composites a cross-origin document (CORS-gated, but the *request* happens and the content can be used in an exfil-via-canvas if the document were *same*-origin-tainted — simple rule: **no external vector references in production UI**, ever).
- The practical line: **inline SVG you authored = trusted surface; SVG from data = render via `<img>`/mask or sanitize; the "just paste the SVG from the user" = an incident waiting.**

## The failure index (symptom → cause → fix)

- *Icon invisible in dark mode* → hardcoded `#000` fill → `currentColor` (or an `id`-scoped `fill` the CSS overrides).
- *Icon tiny/huge in the UI* → missing `viewBox` (or the CSS width not applied because the `width` attribute wins in some contexts) → viewBox + CSS em-sizing.
- *Second icon on the page loses its gradient* → two inlines with the same `id="grad"` → unique-ify per instance / the `symbol`+`use` pattern.
- *Icon blurs / looks "fat" next to the set* → 1px stroke on even coords, or the stroke-width outlier → half-pixel snap / match the set's spec.
- *The toolbar icon is mush at 16px* → one-size asset for all slots → the 16/24/32 optical variants.
- *One icon "breaks the row"* → off-set icon (different pack/grid/cap-style) → the row test, then redraw to the set's spec.
- *The animated SVG janks* → an in-file `<filter>` or a huge path re-drawn per frame → move the effect to CSS, simplify, or a `will-change: transform` *only* on the moving group (`motion`'s compositor rule).
- *The "user-uploaded logo" corrupts the page / runs JS* → inlined untrusted SVG → `<img src>` (frozen) or DOMPurify-SVG; never raw-inline.
- *The sprite works until the SPA route renders an icon twice* → the `symbol`/`use` with a static `id` → per-instance `id`s or inline-from-build.
- *"Can I use an icon font?"* → the 2025 answer is no (per the table) → the inline/sprite/mask, with the a11y the font can't give.
