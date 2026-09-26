# Build order

## 1. Tokens

Start from `tokens.css` and turn measurements into a system:

- **Color roles**: background, surface (and surface-2), text, heading, muted text, primary, on-primary, border, dark surface and on-dark. The map names each with its evidence (button fill, card fill, body ink). Keep four to eight roles; map near-duplicates to one role. Check body text contrast is at least 4.5:1 and large text at least 3:1 (the analyzer lists failures under `accessibility`).
- **Spacing**: pick the base unit the map reports (4 or 8) and a short scale (for example 4, 8, 12, 16, 24, 32, 48, 64, 96). Snap measured gaps to it.
- **Type**: the map's named levels (`display`, `h1`… `body`, `small`) with sizes and line-height ratios. Round to a scale; body 16–18 px, 1.5–1.7 line height, 60–75 character measure.
- **Radius and elevation**: at most two radii and two shadow levels.
- **Container**: the map's `layout.container.width` (content width) and side margins; gutters from `layout.columns`.

## 2. Structure

Semantic landmarks in reference order: `header` (navigation band), `main` with one `section` per band, `footer`. One container class. Grid or flex per band from `layout.columns` (a three-card band is a three-column grid collapsing to one column on phones). No fixed heights: height comes from content and padding. Section padding from the spacing scale, two values at most.

## 3. Components

The map's `components` list repeated blocks: `3× card 380×360 #f8fafc inset 30/30 [1×icon + 2×text]` is one card component used three times. Build each once, parameterized by content. Buttons: primary (filled with the primary color), secondary (outline or surface), quiet (text); one height per tier and a visible focus ring.

## 4. Content and assets

Real copy (OCR text from the map when the mockup's copy is final; otherwise the user's or clearly marked placeholders), then the assets from `image_crop` and `image_trace` moved into the project's asset folder with sensible names. The `.pi/design` artifacts are scratch and git-ignored.

## 5. States and responsiveness

Hover, focus-visible, active, disabled; loading, empty, error; navigation collapse; touch targets of at least 44 px; reduced motion. The reference shows none of these, so derive them from the system.
