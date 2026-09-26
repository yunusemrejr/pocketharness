# Visual language: concepts as working systems

## Turn ideas into visual systems

Ask of every beat: *what would the viewer see if the idea were a machine?* Then show the machine running.

| Idea type | Visual system | Motion that explains |
| --- | --- | --- |
| History, evolution | Timeline axis with the camera travelling along it; later eras inherit shapes from earlier ones | Pan along time; morph an earlier diagram into its successor |
| Architecture, pipeline | Graph or block diagram that assembles | Data packets travel edges in the direction of computation; blocks light up when used |
| Neural nets, math | Layers of nodes, matrices, vectors as arrows | Signal waves propagate; matrix cells fill as products are computed; values change color by magnitude |
| Comparison | Two systems side by side with the same scale | Identical input enters both; the difference emerges in the output |
| Scale, growth | Bars, particles, area that grows to scale | Particles gather into a count; a bar grows past a reference line |
| Code, algorithms | Short code plus its live effect | Highlight the executing line while the data structure changes beside it |
| Relationships, attention | Arcs between elements, thickness = strength | Arcs grow outward from the focus element; focus moves one element at a time |

Primitives in the template: `Stage`, `Backdrop`, `ParticleField`, `Heading`, `TokenRow` (sentence tokens with attention arcs and scan), `NeuralNet`, `Matrix`, `Graph`, `BarChart`, `TimelineAxis` and `CodeBlock`. Extend or compose them before inventing new drawing code. Add a new primitive when a visual will recur; keep it parameterised (data in, progress values in, no timing inside).

## Motion semantics

- Motion must answer "what changed and why". Movement without meaning (bobbing icons, perpetual rotation, gratuitous parallax) is noise.
- Direction encodes causality: inputs enter from the left or top, results emerge right or bottom, and arrows grow from cause to effect.
- One focal change at a time. Stagger groups (3–6 frames per item) so the eye can follow order.
- Entrances: 12–24 frames with ease-out. Exits: faster than entrances. Emphasis: scale ≤ 1.08 or a color shift, never shaking.
- Hold after a reveal long enough to read it: about 0.3 s per word of on-screen text, 1.5 s minimum for a diagram.
- Carry objects across cuts when the idea continues. Reuse positions or morph shapes instead of a fade to a new layout.
- Animate cameras slowly (pans over 2–5 s, ease-in-out) and only to reveal or follow.

## Composition and typography

- Title-safe margin: 120 px at 1080p (the `Stage` default). Nothing essential outside it.
- One focal point per frame. Put the subject on a third or the center, not scattered.
- Text is supporting evidence, not the medium. On-screen text ≤ 8 words per beat. Never put the narration on screen as paragraphs.
- Type scale at 1080p: display 110–140, title 80–96, heading 56–64, body 38–44, labels ≥ 26. Anything smaller is illegible on a phone.
- At most two families (display + text) plus mono for code and data. Weight and size carry hierarchy; do not add colors to create it.
- Use an 8 px spacing rhythm. Align to a small set of edges and keep consistent gutters between scenes.
- Contrast: body text ≥ 7:1 against its background, graphic strokes ≥ 3:1. The backdrop stays quiet (below ~8% contrast).
- Color: one accent for "the thing we're talking about", a second accent only for contrast or warnings. Keep the palette in `video.json` `theme`.

## Anti-slideshow checklist

Reject a beat if any of these hold:

- The frame would work as a static slide with no loss of meaning.
- Bullet lists, paragraphs or more than one sentence on screen.
- The same layout repeated for three beats in a row.
- Decorative motion unrelated to the claim.
- A diagram appears all at once instead of being built in reading order.
- Numbers without a visual scale.
- Stock-style decoration: generic gradients, glowing orbs, emoji or icon soup.
