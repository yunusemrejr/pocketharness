---
name: key-visual-art-direction
description: Art-direct and produce premium brand key visuals — glossy or frosted-glass 3D marks, volumetric light, soft long shadows, gradient stages — as stills, loops, intros, ad sizes or website hero motion. Use when a request wants something that looks special, cinematic or high-end (hero image, banner, launch visual, logo reveal, intro, motion ad) rather than a plain UI.
---

# Key visual art direction

A key visual is the one image a brand, launch or page is remembered by. It earns that by **one idea, lit well**: a single hero form, a deliberate light, a restrained palette and finish. Everything below serves that; decoration that does not is removed.

## Decide the idea before touching tools

1. **Subject**: what is the hero form? Prefer the brand's own mark or a shape derived from it (a letter, an arc, the product silhouette). A generic sphere/blob is the default of every AI image; only use abstract primitives when the brand has nothing better, and then make the composition carry meaning (two forms in tension, a form breaking a grid).
2. **Metaphor of the light**: the light tells the story — a shaft through a window (arrival, clarity), a rim light from behind (reveal, mystery), a low raking key (craft, texture), an even softbox (calm product truth). Pick one and state it.
3. **Palette**: one dominant hue family plus one accent at most. Build it as a value ramp: deep ground (10–20% luminance), mid wall, bright pool, near-white highlight. Monochrome ramps (the violet reference look) read premium because light, not hue, does the work. Check the brand's colors first; the reference image's palette is context, never a template.
4. **Format and crop**: decide the delivery sizes up front (e.g. 1600×640 banner, 1920×1080 intro, 1080×1350 feed, 1200×630 OG). Compose inside the tightest crop's safe area; text and the hero form never touch an edge.

## The craft recipe (what makes it look expensive)

- **Material**: frosted glass = transmission 1, roughness 0.15–0.3 (frost), clearcoat for the crisp rim highlight, a little thin-film iridescence (0.2–0.4) for the colored sheen at grazing angles, attenuation tinted by the body color. Too clear reads as plastic; too rough reads as milk. Metal = metalness 1, low roughness, needs something bright to reflect. Matte walls stay rough (≥0.85) so they hold light pools.
- **Light**: one key with a shape (a spot with a soft penumbra, or a directional through a beam), one weak fill (hemisphere at 0.4–0.7 so shadows keep color, never gray), optional colored accent from the opposite side. The key must create a visible *pool* on the backdrop and a *long soft shadow*; the shadow is half the composition.
- **Atmosphere**: a volumetric shaft (additive beam along the key's direction), a gradient backdrop with a soft glow where the light lands, then bloom on highlights only, subtle film grain (also kills gradient banding) and a vignette to hold the eye in the center.
- **Composition**: the hero sits slightly off-center or on a strong axis with generous negative space on the shadow side; the light travels diagonally across the frame (upper-left to lower-right is the natural reading direction). One figure pops; nothing else competes.
- **Motion (loops, intros, ads)**: slow, weighty, eased moves — a 5–10° turn of the hero, a slow push-in, a light sweep across the glass. 4–8 s seamless loops (last key equals the first). Intros: dark → light reveals the form → hold 1.5 s on the resolved frame → type in. Never spin a logo on its axis at constant speed.

## Produce it with the harness

The local 3D studio renders real WebGL with physical glass; nothing is uploaded and no image generator is involved.

```js
tool_search({ names: ["scene_create", "scene_render", "video_compose"] })
scene_create({ preset: "keyvisual" })            // frosted-glass mark, spot pool, beam, soft shadow, bloom/grain/vignette
// Edit scene.json: replace the mark with the brand's form (arch/disc/capsule/torus/box parts, group + parent),
// set backgroundGradient, wall color, spot position/target, beams, post; keep tracks for a loop.
scene_render({ path: "./media-<id>/scene.json", mode: "frame", time: 1.5 })   // LOOK at the poster, fix, repeat
scene_render({ path: "./media-<id>/scene.json", mode: "video" })              // loop or intro plate
```

Scene vocabulary: styles `luminous` (glass by default) or per-object `material: glass | matte | metal | emissive`; geometry `arch` (half torus, `tube`, `arc`), `disc`, `capsule`, `torus` (`tube`, `arc`); lights `spot` (`target`, `angle`, `penumbra`), `directional`, `point`, `hemisphere`; scene `backgroundGradient {from,to,angle,glow}`, `beams [{from,to,width,color,intensity}]`, `softShadows` (2–6 for crisp-soft, higher only for tiny shapes), `post {bloom, bloomRadius, grain, vignette}`.

Other deliverables:
- **Intro/ad with type and sound**: render the 3D plate, then finish in `code-first-video`/`remotion-video` (kinetic type, sound design) or `video_compose` for cuts and audio.
- **Website hero**: ship the rendered loop as an `<video autoplay muted loop playsinline>` with a poster, or rebuild the look live with Three.js (`threejs` skill) only when interaction is the point; respect `prefers-reduced-motion` (show the poster). See `web-effects` and `motion` for UI-level motion.
- **Stills for social/OG**: render per crop at native size; do not upscale.

## Review loop (every render)

Look at the actual pixels, not the JSON:
- Squint test: does exactly one form pop, and does the light path read in one second?
- Value check: are there true deep darks and one near-white highlight? Flat mid-value frames are the most common failure.
- Shadow: soft but shaped (you can recognize the form in it), falling on the negative-space side.
- Glass: visible rim highlight and refraction of the backdrop; not a flat tinted disc.
- Banding or noise: gradients smooth (grain on), no swimming grain in video.
- Brand fit: colors and form belong to *this* brand; a stranger could not swap in another logo without the image losing meaning.

## Anti-slop for visuals

Reject: the default purple-to-pink gradient blob with no subject; generic floating spheres; lens flares and sparkles as decoration; neon glow on everything (bloom on everything = bloom on nothing); centered-everything with no light direction; mismatched shadows; stock "3D icon" clay figures; text baked into the render (keep type live and editable); motion that loops with a visible jump. Each element must have a job: light, form, space or story.
