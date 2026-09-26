---
name: web-effects
description: >-
  Visual effects for the web — the named effect families and how to build each (glass/blur, grain & noise, spotlight/beam cursor, morphing blobs, liquid/displacement, starfield/parallax, aurora/nebula, chromatic aberration, glitch, mask-reveals, tilt cards, shimmer/shine, spotlight borders), the delivery ladder (pure CSS incl. @property + backdrop-filter + masks → Canvas 2D particles → WebGL/GLSL full-screen shaders → WebGPU), the effect libraries (particles: tsparticles/canvas-confetti; shaders: three.js + postprocessing, OGL, raw WebGL2/WebGPU one-file heroes; SVG filters via CSS filter:url()), and the doctrine that keeps effects from killing perf/a11y/taste: compositor-only, DPR-capped canvas, quality scaling, visibility-gating, reduced-motion branches, the WCAG 3Hz flash rule, text-safe zones, and one-effect-per-screen discipline. Use when adding or debugging a "hero effect" or any visual FX, when an effect janks/overheats on mobile, or when naming what someone else's effect actually is.
---

# Web effects (visual FX)

The job is **visual atmosphere + input feedback**, and the discipline is that effects are the single biggest perf/a11y/taste liability a page can take on. `motion` owns *when/how* (timing, easing, restraint); `threejs` owns real 3D scenes; this owns the **FX layer**: the effect itself — the glass, the grain, the shader hero, the cursor beam. The three failure modes: the *perf* (blur-eats-CPU, shader-melts-phone), the *a11y* (strobe, vestibular, the text-under-effect), and the *taste* (the carnival: everything glowing everywhere).

## The named families (what it's called → how it's built → the cost)

| Effect | The build (cheap → capable) | Cost tell |
| --- | --- | --- |
| **Glass** (glassmorphism) | `backdrop-filter: blur()` + translucent bg + 1px border | blur = **per-frame raster** on scroll; the #1 jank source |
| **Grain / noise** | CSS: SVG `feTurbulence` data-URI overlay (static) or a tiny animated offset; the `mix-blend-mode: overlay` layer | cheap if *static*; the *animated* grain = a repaint per tick |
| **Spotlight / beam** (cursor-follow glow) | a `radial-gradient` at `--mx/--my` CSS vars (mouse sets the vars; the gradient follows — zero JS per frame beyond the var write) | cheap; the naive per-pixel JS does the same thing expensively |
| **Morphing blob** | CSS 2D `border-radius` keyframes (the classic); SVG path morph (same command count); shader metaballs (WebGL) | CSS radius = free; path morph = small; shader = the fill-rate |
| **Liquid / water** (displacement) | SVG `feDisplacementMap` on the image (CSS `filter: url(#…)`) or a WebGL post-process | displacement = per-pixel GPU work; fine on a *hover region*, not full-page |
| **Starfield / depth parallax** | layered CSS transforms on scroll (`animation-timeline: scroll()` — zero JS) / canvas 2D starfield / a shader | CSS layers free; canvas stars cheap with DPR cap; shader the richest |
| **Aurora / nebula** | huge blurred radial/conic gradients + `hue-rotate` anim; or a fbm-noise shader | the CSS version: the blur is the cost; the shader: fill-rate |
| **Chromatic aberration** | RGB split: 3 offset copies (DOM), `feDisplacementMap`, or a 2px `text-shadow` RGB trick | DOM split = layout-ish; shader = one-tap |
| **Glitch** | clip-path slices + RGB offset keyframes (the CSS-only classic, 30 lines) | cheap; **watch the 3Hz rule** (below) |
| **Mask reveal** (text/image slide-in) | `clip-path: inset()` animation (or a moving mask layer) | free (compositor) — the *reveal* of choice |
| **Tilt card** | `perspective` + `rotateX/Y` from pointer position (transform-only) | free; the shadow-follow is the expensive half (fake it with a transform'd blob, not a real shadow anim) |
| **Shimmer / shine** (skeleton sweep, button glint) | a gradient layer translated across (`transform` on the pseudo, not `background-position` — that repaints) | free if transform-driven |
| **Spotlight border** (glowing animated ring) | conic-gradient + a rotating `@property --angle` (the animatable-custom-property trick) or a masked duplicate; `@property` makes the angle *interpolatable* | free with `@property`; without it the conic jump is the fallback |
| **Confetti / burst** | **canvas-confetti** (tiny, the standard) / hand-rolled canvas 2D (30 lines) | trivial; the cap is the *count* (500 particles, 2s, done) |
| **Particles (ambient)** | **tsparticles** (batteries-in, config-driven) or vanilla canvas 2D (the honest case is 40 lines) | the *count × DPR × fill* product; scale it (below) |

**When the effect is a full hero background** (liquid hero, shader nebula, starfield depth): the choice is **Canvas 2D** (simple starfields/confetti), **WebGL fragment shader** on a full-screen quad (the rich organic motion — the "shadertoy-port" category, raw WebGL2 in one file or via **OGL** (the minimal engine) / **three.js** + a `RawShaderMaterial` plane), or **WebGPU** (the modern raw-WGSL one-file hero — same shape, the newer API; ship the WebGL2 fallback for Safari's WebGPU timeline). The doctrine for all: **render at capped resolution** (the fragment shader's cost is *pixels*: a 3× mobile at full res is the thermal brick; 0.5–0.75× on mobile is *invisible* for these effects and halves the work), **pause when the tab is hidden or the hero is off-screen** (rAF auto-throttles the hidden tab but *not* the off-screen-in-page hero — gate it with `IntersectionObserver`), and the **on-demand render** when the static state is the baseline (the "idle = 1 frame, input = animation" pattern cuts the steady-state GPU to zero).

## The perf doctrine (FX-specific)

1. **Compositor-only for DOM FX**: `transform`/`opacity` (and `mask` where supported). The FX jank on scroll is almost always a **blur** (the `backdrop-filter` re-rasters its backdrop *per frame*) or a **gradient/`background-position` anim** (repaint) — the fixes: the opaque scrim *behind* the glass instead of live-blurring complex content, the pre-blurred static backdrop, the transform-driven shimmer instead of the position-driven one. The `filter: url(#svg-filter)` on *text* during motion = the "text blurs on hover" bug (swap to transform/mask, or freeze the filter at rest).
2. **Canvas DPR cap**: draw at `devicePixelRatio` **capped at 2** (the 3× retina canvas is 9× the pixels of CSS space for zero visible gain); resize-handling must reallocate the buffer (the "particles look tiny after rotate" bug is the missing DPR re-fit).
3. **Quality scaling**: start from a *baseline* particle count / shader resolution and *measure the first second* (the FPS probe: < 50fps for 2s → halve count / resolution). The `navigator.hardwareConcurrency` + the mobile UA are priors, but the *probe beats the priors* — the 12-core that chokes is the iGPU-macbook case the priors miss.
4. **Context discipline (WebGL/WebGPU)**: **one context** (browsers cap live WebGL contexts ~16; two canvas FX on a page = the classic leak path), **dispose** on route change (the GPU memory — the `threejs` disposal rules apply to raw shaders too: the textures/programs/buffers), and **handle `webglcontextlost`** — the "hero goes black after the laptop sleeps" is a context loss *without a restore handler*; the restore = re-init the GL state on `webglcontextrestored`.
5. **The battery test**: the effect at rest should cost *zero* (static frame, no rAF loop spinning) — the perpetual 60fps ambient FX is the "why is my fan on" bug; idle-stills + input-driven wakeups is the pattern (the `animation-libraries`/`threejs` on-demand-render rule, generalized).

## The a11y doctrine (FX-specific)

- **`prefers-reduced-motion`: the branch, not the dial.** The animation FX → *its* still frame (the aurora → a static gradient, the starfield → a still composition, the glitch → the base state). "Reduced" (20%-speed) fails both the vestibular users *and* the design (20%-speed glitch is worse, not better). The reduced version must be *complete* — the whole message present in stillness.
- **WCAG 2.3.1 (the 3Hz rule)**: no content flashes more than 3×/second — the **glitch / strobe / RGB-flicker effects** are the classic offender (a 10Hz slice-clip glitch is a seizure hazard, a legal exposure, and a design failure at once). The animatable check: count the light/dark *transitions* per second in the worst case; the glitch FX's "fast" preset needs the cap *or* the reduced-motion branch killing it outright.
- **Text-safe zones**: no blur/noise/grain/hue-anim *under running text* — the legibility dies (the contrast of *translucent* text over a moving bg is a moving contrast; the rule: the reading text sits on a **stable, high-contrast surface**; the FX is the *margin*, the *hero-behind*, the *page-edge*, never the paragraph substrate). The grain overlay *over everything* (the 3-5% opacity site-wide noise) is the one exception — it's *flat*, adds texture, doesn't move under the text (static-only rule).
- **Vestibular**: the 3D parallax/tilt is the dizziness source — the depth *budget* (parallax ≤ a small translate, tilt ≤ ~8°) + the reduced-motion kill. The fullscreen-scroll-driven zoom is the "car-sick hero" — the dead tell for overreach.
- **Screen readers**: the FX is a *visual-only channel* — nothing load-bearing (state, meaning, content) may live only inside the effect (the "the loading state is only the spinner-glow" bug; `motion`'s non-motion-channel rule applied to FX).
- The `prefers-contrast: more` / `forced-colors` Windows mode: the **grain + the glass translucency dies** in forced-colors by design (the OS kills it) — accept the degradation, don't fight it; the spotlight-border's glow is *lost* there (the border color remains — the semantic survives, the ornament doesn't: the correct outcome).

## The taste doctrine (why tasteful FX doesn't look like a theme park)

- **One effect per screen, one job each**: the hero gets *the* effect; the sections get the *composition* (the `frontend-design` budget: ≤1 decorative element per screen — the FX slot is that element); a second hero-grade FX on the same screen = the carnival (the grain + the aurora + the cursor-beam + the glitch cards = the 4-flavor mess; pick the one that carries the brand).
- **The effect is *input-woken*, not perpetual**: the quiet baseline, the *proximity* drives the intensity (the cursor-beam brightens near the cursor; the blob responds to hover) — the always-at-full FX reads as noise with time on it (the amplitude decays with lack of input — the *reactive* FX is the modern one, the *looping* FX is the 2021 one).
- **The mobile is the composition, not the demo**: the FX is the *desktop bonus*; on mobile the *still frame* is the design (the hero's static state must be beautiful on its own — that's the test: screenshot it paused, mobile viewport, does it survive? If the effect *is* the design (the still is empty), the design is the effect and the effect failed).
- **FX serves a named job**: atmosphere (the brand mood — the aurora = "we're cosmic/futuristic"), feedback (the hover glow = "this is alive and responsive"), narrative (the scroll-driven FX = the story beat — pairs with `storytelling`/`animation-libraries`' ScrollTrigger). The "because it's cool" FX is the one that survives no remove-test (`frontend-design`).
- **The restraint ladder** (where to spend the FX budget): the *hero* > the *interactive elements* (hover/drag feedback) > the *transitions* (between states) > everything else (nothing). The footer-confetti and the nav-glow are the *negative*-value slots.

## The failure index (symptom → cause → fix)

- *The glass panel janks on scroll* → per-frame `backdrop-filter` re-raster → the opaque/semi-opaque scrim behind, the pre-blurred static backdrop, or the blur only where the backdrop is *simple* (a solid-ish gradient region — the blur cost is in the *content behind*).
- *The particles look tiny/sparse after rotation or window resize* → missing DPR re-fit → reallocate the canvas buffer on resize (with the DPR cap at 2).
- *The hero goes black after sleep / after GPU swap* → WebGL context loss unhandled → the `webglcontextlost` (preventDefault) + `webglcontextrestored` re-init.
- *Second canvas FX on the page kills the first* → the ~16-context cap / double GPU load → one context (draw both FX from one GL surface) or one FX (the taste rule: the answer is usually delete).
- *The mouse-hover text goes blurry* → `filter: url(#…)` or a blur over the text during motion → transform/mask instead; the filter at *rest* only.
- *The shimmer button repints the whole row* → `background-position` anim → the `transform: translateX` on a gradient pseudo-element (the compositor).
- *The site-wide grain causes a repaint storm* → animated noise full-page → the **static** grain overlay (the 3-5% opacity, zero animation) — the texture is there, the cost is gone.
- *The phone melts on the shader hero* → full-res fragment fill on a 3× display → the resolution cap (0.5–0.75×), the DPR-aware render target, the quality probe (halve in 2s if < 50fps), the still-fallback for the thermally-constrained (the laptop-on-AC case reads the same).
- *The glitch trigger complains / the motion-sick user bounces* → the >3Hz flash → the 3Hz cap on the transition count *and* the `prefers-reduced-motion` branch killing the effect (the still base state).
- *The hero is beautiful on my MacBook, dead-simple on every phone* → the effect *is* the design (the still is empty) → redraw the still frame as a real composition (the `frontend-design` texture-budget: the still is the design, the FX is the garnish).
- *Two "subtle" effects that look like nothing individually, everything together* → the carnival accumulation → the one-effect-per-screen rule, the remove-test (the `frontend-design` audit).
- *The confetti/burst lingers or double-fires* → no lifecycle (the replay-on-re-render in React) → single-shot with the cleanup, the idempotent trigger, the 2s max lifetime, the count cap.
- *"Can I use backdrop-filter everywhere?"* → no: the cost is per-frame and per-surface-area (the fullscreen glass = the tax) → the *region* glass (panels, headers), the opaque-scrim alternative for the rest, and the Safari/mobile check (the `backdrop-filter` is shippable now but the *low-end Android* blur cost is real — the probe, the fallback = the opacity scrim).
