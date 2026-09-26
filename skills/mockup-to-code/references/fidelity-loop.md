# Fidelity loop

`visual_diff` compares at CSS resolution. Give it the reference and either a screenshot (`candidate`) or the page (`source`: an HTML path or a dev-server URL). It renders at the reference's CSS width with a desktop, tablet or phone viewport height, captures long pages in 4000 px slices, and stitches them.

## Reading the result

- **verdict** `close` (SSIM ≥ 0.95, under 3% changed), `similar with visible differences`, `noticeably different`, `substantially different`. Close is the goal, not zero.
- **heightDelta** and **shifts**: `s3 (y700) matches the candidate 40px lower` means everything from that band down sits 40 px too low. The cause is above it: padding, margins, line heights or an extra element. Fix shifts from the top down and re-run before touching anything else, because every later region inherits the drift.
- **worstSections**: bands ranked by mean perceptual difference times height. A band with high `changed` but no shift has wrong content, color or layout inside it.
- **regions** with `zoom` images (reference left, build right) and `referenceBlocks` (with `map`): look at the zooms before editing.
- **missingColors**: reference colors absent from the build (a surface tint or accent you dropped). **extraColors**: colors the build introduced.

## What legitimately differs

Font rasterization and anti-aliasing, placeholder images, dynamic content, and fonts that are close substitutes. Do not add hacks to chase these; say so in the result.

## Iteration discipline

One class of fix per iteration (vertical rhythm, then widths and columns, then color, then type details), re-render, re-compare. Keep the previous `diff.json` to show progress. When a region will not converge, compare just that region (`region` in reference pixels) or zoom with `render_see` `selector`.

## Beyond one width

The reference fixes one viewport. After the diff converges, render phone (390), tablet (768) and wide (1440+) widths with `render_see` and judge them against the system, not the reference: no horizontal overflow, readable measure, touch targets, a navigation that collapses deliberately.
