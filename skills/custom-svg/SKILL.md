---
name: custom-svg
description: Hand-crafting SVG — drawing paths, building icon sets, gradients, masks, filters, symbols, animation, accessibility, and clean export pipelines. Use when creating or fixing SVG illustrations, icons, logo work, or inline SVG in UI.
---

# Custom SVG Design

## Hand-drawing paths

- Always set `viewBox="0 0 24 24"` (or your grid) and DON'T set fixed width/height when it scales in context — or set both so it behaves inline.
- Command fluency: `M` move, `L` line, `H/V` horizontal/vertical, `C` cubic (two control points + end), `Q` quadratic (one control + end), `A` arc (rx ry rotation large-arc sweep x y), `Z` close. Lowercase = relative to current point (better for icon work — paths stay movable).
- **Design on a grid:** 24×24, 2px stroke, live area 20×20 (2px optical pad). Round caps and joins for friendly sets, square/miter for technical. Align key points to integers or halves — sub-pixel drift is the #1 amateur tell.
- Symmetry beats hand-tweaking: mirror control points (e.g. `C 6 8, 8 6, 12 6 C 16 6, 18 8, 18 12`) instead of eyeballing.
- Keep one consistent visual weight across an icon set: same stroke width, same corner radius, same mass (a thin "plus" next to a fat "hamburger" looks broken).
- Optically correct, not geometrically: overshoot circles/diamonds past the grid by ~1–2% (true circles read small); terminals get a hair more weight.

## Fills vs strokes

- Stroke icons: `fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"` — inherits text color, recolors for free.
- Fill icons: use `fill-rule="evenodd"` for donut holes (inner contour = hole).
- Multi-tone marks: separate paths per tone, never gradients inside tiny icons.

## Gradients, masks, filters

- `<linearGradient>` in `<defs>`; `gradientUnits="userSpaceOnUse"` for precise control; gradient stops in oklch/hex both fine.
- Masks = luminance (`<mask>` with white/black), `clipPath` = geometry only — clip is cheaper, use clip for shape cropping, mask for soft falloff.
- Filters: `feDropShadow` for shadows (cheap, GPU), `feGaussianBlur` for glows. Filters are rasterized — they break crispness on scale and 1px strokes; avoid filters on icons, use them sparingly on illustrations.
- Every `id` in inline SVG is global to the document — prefix ids (`icon-arrow-gradient`) or you'll have cross-icon bleed when multiple inline SVGs share `#grad1`.

## Reuse & systems

- Pattern: build all icons as `<symbol id="...">` in one `<svg style="display:none">` sprite, reference with `<svg><use href="#icon-x"/></svg>`. Single source, one HTTP weight, CSS-recolorable via `currentColor`.
- SVG-in-CSS (`url(logo.svg)` in `content` / `background`) loses `currentColor` — inline it in the component instead.
- Minimize: run **svgo** (`svgo --multipass --precision=1 file.svg`) — strips editor namespaces, precision cruft; Figma/Illustrator exports carry 5–20× bloat.

## Animation

- CSS `transform` on the whole `<svg>` or `<g>` (GPU, smooth); CSS on path geometry (d-attribute) only in modern browsers — use `pathLength="1"` + `stroke-dasharray: 1; stroke-dashoffset: 1→0` for the universal draw-in line effect.
- `transform-box: fill-box; transform-origin: center` so rotations pivot on the shape, not the viewport corner.
- SMIL (`<animate>`) still works everywhere but avoid mixing SMIL + CSS on the same element.
- Respect `prefers-reduced-motion`: stop autonomous loops.

## Accessibility

- Decorative icons: `aria-hidden="true"`.
- Meaningful standalone SVG: `role="img"` + `<title>` (first child) + `aria-labelledby`.
- Interactive: wrap in `<button>`, focus on the button, don't hand-roll focus on the svg.

## Pitfalls

- Embedded rasters (base64 PNG inside "SVG") — replace or accept the weight.
- Strokes not set `vector-effect="non-scaling-stroke"` when the icon scales wildly.
- `100%` sizes without aspect-ratio context → layout collapse; give the container `aspect-ratio` or explicit box.
- Forgetting that `filter: drop-shadow()` (CSS) on the svg element is cheaper and crisper than an SVG filter for simple cases.