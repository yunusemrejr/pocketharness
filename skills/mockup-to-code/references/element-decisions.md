# Element decisions: text, CSS, SVG or raster

Decide per element, from what it is rather than how it looks. The analyzer's `implement` field (`text`, `css`, `css+children`, `svg`, `raster`) is a starting point; the overlay shows where it may be wrong.

| Reference element | Build it as | Why and how |
| --- | --- | --- |
| Any readable text, including button labels, nav items and numbers in charts | HTML text | Accessible, translatable, selectable, crisp at every density. Font size: the map's `fontSize` is ink height / 0.93, within about ±15%; all-caps lines read about 20% small. Line height: the block's `lineHeight` (pitch) when it has several lines. |
| Flat panel, card, band, pill, input | CSS box | `background`, `border-radius` (map `radius`), `border` (map `border` for outlined panels), padding from the component's `inset`. One card spec for the whole site. |
| Gradient | CSS `linear-gradient` | Map `gradient.from/to/angle`; add a middle stop only if the ramp visibly bends. Radial glows are `radial-gradient` at low alpha. |
| Shadow | CSS `box-shadow` | Estimate offset and blur from the soft edge's width; use one or two elevation levels site-wide. |
| Divider | CSS border or `<hr>` | Low-contrast rules are often borders of adjacent panels, not separate elements. |
| Icon (small, few colors) | SVG | First choice: the project's icon set or a known family that matches stroke weight and corner style. Second: `image_trace` (verdict `good`, then set `fill="currentColor"` when the icon inherits text color). Never a PNG sprite. |
| Logo | SVG | Ask for the real logo file; a trace is a placeholder. `image_crop` with `key:"auto"` and `trim:true` gives a transparent raster stand-in. |
| Photograph, painterly illustration, 3D render | Raster (WebP or AVIF) | `image_crop` (`kinds:["image"]`). Serve with `width`/`height`, `srcset` at 1x and 2x, `loading="lazy"` below the fold, meaningful `alt` or `alt=""` when decorative. Mockup crops are placeholders by default. |
| Flat illustration, blob, wave, pattern | Inline SVG or CSS | Trace if flat (`image_trace` with more colors); CSS gradients, `mask` or repeating backgrounds for patterns. |
| Device frame, browser chrome | CSS or SVG | Build the frame, put a real screenshot inside. |
| Chart | Chart component or SVG | Rebuild from data; never ship a picture of a chart. |
| Video thumbnail with play button | `<video>` poster or button over an image | Interaction and accessibility come from the element, not the picture. |

## When the analyzer is unsure

- `mixed` blocks need a look: an illustration with text inside is two layers (raster or SVG art plus HTML text); a composed widget is several CSS boxes.
- A text line classified as `icon` is usually a fragment of large type: treat the whole line as text.
- A section marked `busy` is a full-bleed photo or pattern: background image with the content layered on top, and a scrim if text sits on it (check contrast on the busiest area).
- Subtle surfaces (a 2% tint) are real design decisions: keep them as surface tokens instead of dropping them to white.

## Fonts

Identify the family by structure: geometric sans (circular o, single-storey a: Inter-like or Poppins-like), grotesque (Helvetica-like), humanist (Source Sans, Open Sans), serif families by bracket and contrast, monospace. Prefer the project's existing fonts, then a close open-license family. Match x-height and width before exact shapes; a narrower substitute changes line breaks and every height below it. Load at most two families, with `font-display: swap` and the weights actually used.
