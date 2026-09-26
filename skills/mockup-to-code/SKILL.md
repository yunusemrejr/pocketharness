---
name: mockup-to-code
description: "Turn design references (mockups, screenshots, Figma or Dribbble exports, image URLs) into real, responsive, accessible code, or restyle an existing site to look like a reference. Recovers the design system from pixels with image_analyze (bands, components, palette roles, type scale, spacing, tokens), decides text vs CSS vs SVG vs raster per element, cuts and vectorizes assets (image_crop, image_trace), renders and iterates with visual_diff, and generalizes one or two references into consistent pages the images do not show. Use for 'turn this into a website', 'make it look like this', pixel-perfect or clone requests."
---

# Mockup to code

A reference image is evidence of a design system, not a picture to reproduce pixel by pixel. The job has three parts: recover the system (grid, spacing, type, color roles, components, assets), build it as ordinary semantic, responsive code, and prove fidelity by rendering the build and comparing it with the reference. Code that matches at one width through absolute positioning and fixed heights has failed even when the screenshot matches.

## The loop

1. **Frame the job.** Which references, which pages, which stack, which viewport each image represents. A remote image: pass `url` (downloaded once through the guarded fetcher and kept locally). A live website as the reference: capture it with `render_see` (`fullPage:true`, desktop and phone widths) and analyze the captures. Retina exports: `image_analyze` infers the pixel ratio from common widths (2880 → @2x); pass `scale` when you know it.
2. **Analyze before coding.** `image_analyze` writes `design-map.json`, `overlay.png` (block ids drawn by kind) and `tokens.css`. Look at the overlay beside the reference with vision and correct misreadings in your notes: the analyzer guesses roles and font sizes from pixels. Pass `ocr:true` when the copy matters; recognized text lands on text blocks. Read the map selectively: `sections`, `components`, `typography`, `palette`, `layout`.
3. **Decide per element**, using [element decisions](references/element-decisions.md): text is HTML text; flat panels, borders, radii, shadows and gradients are CSS; icons and logos are SVG (the project's icon set first, then `image_trace`); photographs and painterly illustrations are raster assets (`image_crop`); patterns and decorative shapes are CSS or inline SVG.
4. **Tokens, then structure, then components.** Snap the measured tokens to a coherent system (spacing to the unit, type to a scale, colors to roles) before writing components. Build landmarks and the container/grid first, then the repeated components the map reports (one spec each), then the details. See [build order](references/build-order.md).
5. **Assets.** `image_crop` with `kinds:["image"]` cuts every photograph in one call; `key:"auto"` and `trim:true` cut logos off flat backgrounds; check each file. `image_trace` only for flat marks, and only keep results with a `good` verdict. Crops from a mockup are placeholders unless the user said the mockup contains final assets.
6. **Render and compare.** `visual_diff` with `reference` and `source` (the HTML file or dev-server URL) renders the build at the reference's CSS width, long pages in slices, and reports the verdict, bands displaced vertically, the worst bands, hot regions with zoomed crops and the reference blocks under them, and colors missing or added. Fix height and spacing drift first, from the top down: a shift cascades into every later region. Iterate until the verdict is `close` or every remaining difference is explained. Read [fidelity loop](references/fidelity-loop.md).
7. **Beyond the still.** The image shows one width and one state. Check 390, 768 and 1280 px with `render_see`; add hover, focus-visible, active and disabled states, empty, loading and error states, and reduced motion. Run `design_audit` for contrast, overflow and type drift.

## One or two references for a whole site

Most requests give one or two images and mean the whole site. Extract the system from the references, write it down (tokens plus a short component inventory), then build every other page from it. Classify each page by archetype and compose it from the extracted parts; invent only what the system implies, in its style. Show one derived page before building them all. Read [site-wide generalization](references/site-wide.md).

## Restyle an existing site to "look like this"

Keep the content, structure and behavior; change the system. Analyze the reference, audit the current site (`design_audit`), map old tokens to new ones, apply them at the token layer (CSS variables, Tailwind theme) and then the components, and compare pages whose layout matches the reference with `visual_diff`. For pages whose layout differs, judge by tokens and components rather than pixels. See [restyling](references/site-wide.md#restyling-an-existing-site).

## Failure modes

- Rebuilding the reference's generic SaaS costume (glow, gradient blobs, badge rows, bento filler, chatbot) instead of its system; hold every addition to `docs/ANTI-SLOP-CHECKLIST.md` and the observer-book `anti-slop` passages.
- Absolute positioning, fixed heights or magic numbers to force a pixel match; text rendered as images; tracing type.
- Fifteen nearly equal grays copied from pixels instead of four roles; a new radius or shadow per card.
- Shipping mockup placeholder copy, stock photos or logos cut from the mockup as final assets.
- Chasing zero difference: font rasterization and anti-aliasing differ legitimately. Stop at matching structure, spacing, scale and color.
- Treating the analyzer's guesses (roles, font sizes, components) as facts without looking at the overlay.
