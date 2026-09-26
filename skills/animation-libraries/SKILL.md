---
name: animation-libraries
description: >-
  Which web animation library to reach for and why — plain CSS first (transitions/keyframes, the 80% case), then the library ladder: GSAP (+ScrollTrigger, Flip — the pro page-choreography standard), Motion (motion.dev / framer-motion — the declarative React API with layout/exit animations and springs), anime.js (lightweight tweening + SVG), Lottie vs Rive (designer-authored animation: After Effects JSON vs the interactive state-machine runtime), Swiper/Embla for carousels, Lenis for smooth scroll, the View Transitions API for page transitions, three.js for 3D (`threejs`). Per library: what it's for, the signature features, the bundle cost, and the gotchas (timeline cleanup on unmount, ScrollTrigger refresh, Lottie size bloat, reduced-motion discipline). Use when choosing an animation/motion library for a build, or when an animation is janky/leaky and you need the library-specific failure list.
---

# Animation libraries (the ladder)

`motion` is the *craft* of when/how (duration, easing, restraint, a11y); this is the *tooling*. The ladder, bottom-up, and the rule that holds at every rung: **if CSS does it, there is no library** — a 250ms `transition` or a 3-keyframe `@keyframes` is the only sane choice for 80% of UI motion, and the library for that 80% is a bundle-size + maintenance tax with no return.

## The ladder

| Rung | Tool | The job it owns | Cost | Reach here only when |
| --- | --- | --- | --- | --- |
| 0 | **CSS** (`transition`, `@keyframes`, `animation-timeline`, the `View Transitions API`) | Microinteractions, hovers, reveals, layout/enter exits, page-to-page morphs | 0 | *Always* — the default |
| 1 | **Motion** (motion.dev; framer-motion in React) | Declarative enter/exit/**layout** animations, springs, gesture-driven, orchestration of many elements | ~30-50KB gzip | CSS can't express it (layout-preserving moves, staggered orchestration, the exit animation of a removed node) |
| 2 | **GSAP** (+ ScrollTrigger, Flip, MotionPath) | Page *choreography*: scrubbable scroll sequences, the precise timeline control, the long-form scrollytelling | ~30-70KB (plugin-stacked) | the design demands a scrubbed/precise scroll narrative, or the team wants the most battle-tested engine |
| 3 | **anime.js** | lightweight tweening, SVG path animation | ~7-17KB | the no-framework vanilla case wanting more than CSS, without GSAP's size |
| 4 | **Lottie** (lottie-web / @lottiefiles/dotlottie-web) | *Design-authorship*: the After Effects illustration animation, exported JSON | the **file is the cost** (100KB–2MB per animation) | the designer owns the animation in AE/Figma and the code shouldn't hand-tune it |
| 5 | **Rive** | *Interactive* design-authorship: state machines (hover/press/drag states), tiny runtime | ~100-170KB runtime (+ small `.riv` files) | the asset needs *behavior* (a character that reacts to input), not just playback |
| 6 | **three.js / WebGPU** (`threejs`) | real 3D | ~150KB+ + the GPU | the content *is* 3D — never for "3D-looking" |
| carousel | **Swiper** (batteries-included) / **Embla** (headless, you style) | sliders/carousels | 5-7KB Embla / ~15-40KB Swiper | the pattern needs *its* a11y + gestures + pagination done for you |
| scroll | **Lenis** | smooth/inertial scroll (the "studio site" feel) | ~5-9KB | the *feel* is a brand decision, and you accept the caveats below |
| transitions | **View Transitions API** (native) / **Barba.js** (multi-page fallback) | page-to-page morphs | 0 / ~10KB | the SPA-ish feel on a server-rendered site |

## The named libraries, properly

### CSS (rung 0) — the default, and its real power is *underused*

- The three primitives: `transition` (the property state-change), `@keyframes` (the open timeline), **`animation-timeline: scroll()/view()`** (scroll-driven, *no JS* — the scroll-reveal and the parallax-lite without an observer library; browser-coverage-gated, feature-detect).
- The **View Transitions API**: `document.startViewTransition(...)` gives you the page-morph for free (the shared-element `view-transition-name`) — in 2025 it covers Chrome/Edge/Safari; the fallback is "no transition" (that's *fine* — the graceful-degradation *is* the design).
- What CSS genuinely can't do: an **exit animation of a removed element** (removal kills the animation — the classic "why does it pop?" bug), layout-preserving reflow (the FLIP: measure-first, transform, invert, play — *possible* in CSS but you'll write the same JS as the library), complex multi-segment timelines with sync points, and scrubbable scroll sequences. Those are the *entry tickets* to rung 1/2.

### Motion (motion.dev / framer-motion in React)

- The signature: **declarative + physical**. `animate` prop with a `transition` (the spring config is the default feel — the spring is the "right" easing for UI, `motion`'s rule), `AnimatePresence` for the **exit** animations (the removal-pop fix), the **layout animation** (`layout` / the shared `layoutId` — the element *moves* between parents/states with correct interpolation: the sheet that opens from its trigger button, the card that reflows to the top of the list) — the layoutId shared-element is the feature you pay the bundle for.
- Spring over tween by default (velocity continuity — the "it stops like a robot" tell is a fixed-duration tween); the `stiffness/damping` presets or the named ones over hand-tuning numbers.
- Gotchas: the layout animation on a *large list reflow* janks (measure cost) — scope it to the moving subtree, not the page; the `will-change` overuse is the library's *default* in some setups (the memory on low-end mobile, `motion`'s compositor rule); in React 19/the RSC world the motion components are the client boundary — put them in the leaf, keep the data server-side.
- The "one feature decides" test: if you need *exits + layout moves + springs* in the same component tree, this is the tool and the alternatives (GSAP from React) are fighting the framework.

### GSAP (the choreography engine)

- The signature: **the timeline** — the precise, scrubbed, *addressable* sequencing (`tl.to(...).to(...)` position-parameters, the `.pause()/.seek()`, the **scrubber**) + **ScrollTrigger** (the scroll-position → timeline-progress binding is the industry-standard for the "scroll story" page) + **Flip** (the manual-FLIP: the state-change layout animation, the `state` capture).
- The "why pay" case: the design is a *narrative* — a sequence that is precisely timed, scrubbed by scroll, must be resumable/interruptible, with a specific segment that plays on a click. GSAP is the most battle-tested engine this exists in (a decade of production edge-case fixes).
- Gotchas: **the cleanup-on-unmount** (the React integration's `useGSAP`/the `gsap.context()` + `revert()` — the *manual* timeline in a useEffect without a `revert` is the "animation runs twice / leaks after unmount" bug — the #1 GSAP+React incident); **ScrollTrigger.refresh()** after async content (an image that loads changes the scroll length — the trigger is now in the wrong place; the `invalidateOnRefresh` + the refresh on the resize/load events); the plugin licensing (the "web" plugins are free, a couple of the newer ones are club-licensed — check before you pick a feature); the 3D transform on DOM elements (the perspective handling is good, but the *3D* is a 2D-engine trick — for real 3D, three.js, rung 6).

### anime.js

- The light rung: the tween-with-features (stagger, the SVG `motionPath`-adjacent drawing, the timeline) for the no-framework vanilla codebases. The honest position: for *most* of what anime.js does, CSS + a few lines does it better-specified; the tool wins on the SVG-path drawing and the stagger ergonomics in vanilla. If the project is React, prefer Motion (the framework fit); if it's a vanilla landing, decide CSS-first (rung 0) and take anime.js only for what CSS can't.

### Lottie vs Rive (the design-authorship pair)

- **Lottie**: After Effects (or the Figma plugin / the prototype tools) → JSON → `lottie-web` plays it. The feature set is **playback** (play/pause/speed/segments) + text/color swap. The *cost is the file*: a 5s AE animation commonly lands **100KB–1MB+** (the keyframe data is proportional to the keyframe density — the 60fps "smooth" export is the bloat multiplier; export at the *rendered* fps, not 60, and the plugin settings matter more than people think). Also: Lottie is *passive* — it can't react to user state without the JS reaching in (the segment-swap is the interaction model).
- **Rive**: the *file is a state machine* (`.riv` + the Rive renderer ~100-170KB): the designer builds the *states* (idle/hover/press/drag/input-reactive) and the transitions, and the code drives the state — the **interactive** asset (a mascot that reacts to scroll/input, the onboarding character). The `.riv` files are *small* (often 5-50KB) because the motion is *procedural* (the state machine interpolates; it doesn't store every frame).
- **The decision**: *passive illustration animation in hand-off* → Lottie (if the file is under ~150KB — beyond that, the designer re-cuts it or it's Rive). *Interactive/behavioral asset* → Rive (Lottie can't do it honestly). *You have neither and it's simple* → the Motion/anime.js hand-built version (the 20-line state animation beats a 400KB JSON).
- Shared gotchas: the runtime is **another** JS dependency in a render pass (the low-end mobile budget — `web-performance`); the accessibility (the *meaning* of an animated asset must exist outside the animation — `prefers-reduced-motion` must map to the *still frame*, not "less animation" — the reduced-motion state of a mascot is a *posed* frame, not a 10%-speed ghost).

### Carousels: Swiper vs Embla

- **Swiper**: the batteries-included (pagination, loop, the keyboard, the a11y, the modules) — the "a slider that behaves" in an afternoon, at the price of the opinionated markup + the theme.
- **Embla**: the headless (measure + scroll logic + the events; *you* build pagination/arrows/a11y) — 5KB, full control, the *design-system* answer (the carousel fits *your* tokens, `design-systems`).
- The pattern rule (`web-patterns`): a *content* carousel (product scroller) is fine; a *hero* carousel with auto-advance is the 2010 tell — the auto-advance + the content that changes is the attention tax; the "one hero, n secondary as a static grid" beats it almost always.

### Lenis (smooth scroll) — the judgment call

- What it does: the inertial "studio" scroll (lerp-interpolated). It's the *brand feel* for a specific kind of product (the creative-tool landing, the studio site).
- What it costs: it **hijacks the scroll container** (the native scroll, the overscroll, the anchor-jump, the `scrollTo` — all must go through Lenis or be remapped; the **anchor-link** + the **focus-scroll** + the **native scroll for accessibility** are the three things that break first — keyboard users and screen readers expect *real* scroll); the `100vh`/the sticky positions interact badly (the fractional-viewport jank); the battery on low-end mobile (the per-frame lerp).
- The rule: **Lenis is a brand decision that has a11y + native-behavior costs** — ship it only if (a) the feel is the product, (b) you've wired the anchors/focus/keyboard through it, and (c) `prefers-reduced-motion` *disables Lenis entirely* (the reduced-motion user gets *native* scroll — that's the correct degradation, not "slower Lenis").

## The cross-cutting rules (every library, every rung)

1. **`prefers-reduced-motion` is a feature, not a nice-to-have** — the query-gated *branch* (different behavior, not a scaled-down version of the same behavior): the parallax → none, the scroll-driven sequence → the static composition, the ambient loop → the still frame, Lenis → native. The reduced-motion *version must still be the design* (the content is all present, sequenced to read in stillness — `motion`'s rule).
2. **Compositor-only properties** (`transform`, `opacity`, and the `filter`/`clip-path` with care) — the `top/left/width/margin` animation is the reflow jank, every library included (`motion`'s core rule; GSAP's `x/y` over `left/top` is the same lesson in their API).
3. **The cleanup discipline** is the frontend incident list: the `useEffect` animation without the `revert`/`cleanup` (the double-run + the leak), the ScrollTrigger without the `refresh` (the mis-positioned trigger), the `requestAnimationFrame` loop that never `cancel`s (the background-tab drain — check it in the *other tab*), the `MutationObserver` that re-triggers on its own writes (the infinite loop — the library self-triggering is a classic).
4. **The size budget is enforced at the import, not the build**: the `gsap` core + *named* plugins (the tree-shake is the import list), the Motion's named subpath imports, the Lottie runtime *only* on the pages that have Lottie (the route-split, `web-performance`'s code-split), the Rive runtime lazy (it's the 150KB of *playback*, load it where the asset is).
5. **Don't animate what isn't there**: the off-screen animation (the reveal that fires for no one — `IntersectionObserver`-gated, or the `animation-timeline: view()`), the hover animation in a touch context (there's no hover — the state must degrade to the tap or be absent), the *second* ambient animation when the first is working (two competing attention sources = neither reads).
6. **The a11y floor**: the motion that conveys state (loading, success) must have a *non-motion* channel too (the text appears, the color changes — the animation is the *polish* on the state, not the *carrier* of it); the `@media (prefers-reduced-motion)` + the focus-visible on any motion-triggered control.

## The decision drills (the 5 questions)

1. **Does CSS do it?** (transition/keyframes/View Transitions/`animation-timeline`) → stop here.
2. **Is it a *removed* element's exit, a layout move, or a spring/gesture?** → Motion (the framework-fit).
3. **Is it a *scroll narrative* or a precise multi-segment timeline?** → GSAP + ScrollTrigger (the choreography).
4. **Is the animation *authored by design* (AE/Figma/rive file)?** → passive = Lottie (file-size-checked), interactive = Rive.
5. **Is it *3D*?** → `threejs` (and the *content is actually 3D* test, not "looks 3D").

Anything that answers "none of these" is a CSS you haven't written yet.

## The failure index (jank/leak → cause → fix)

- *The element pops out on remove (no exit animation)* → CSS can't animate a removal → `AnimatePresence` (Motion) / the GSAP `onComplete`-then-remove, or the Web Animations API's `element.animate` + `onfinish`.
- *The scroll sequence is in the wrong place after the content loads* → ScrollTrigger without a `refresh` → `ScrollTrigger.refresh()` on the image-loads/`resize`, `invalidateOnRefresh: true`.
- *The animation runs twice / the memory grows on route changes* → the uncleaned timeline/RAF → the `gsap.context().revert()` / the `cancelAnimationFrame` in the effect cleanup — the #1 leak in the framework + GSAP pairing.
- *The page janks on the list reflow* → the layout animation on the whole list → scope the `layout` to the moved subtree (Motion) / the FLIP on the moving nodes only (GSAP Flip).
- *The "smooth scroll" breaks the anchor links and the keyboard* → Lenis hijacking the native scroll → wire the anchors/focus through Lenis' API (or drop Lenis; the native scroll is the a11y floor).
- *A 600KB "animation"* → the Lottie at 60fps keyframes → re-export at rendered fps / the Rive state machine (procedural = tiny) / the hand-built CSS (the 20-line version).
- *The hover effect fires on touch (or the reveal fires off-screen)* → no context check → the `@media (hover: hover)` gate + the `IntersectionObserver`/`view()` timeline.
- *The animation is invisible to the reduced-motion user, or is "slower ghost"* → the scaled-down anti-pattern → the *branch*: the still-frame/pose version, the content-complete static composition.
- *The second route loads the whole animation engine* → the un-split runtime → the route-split/lazy for the Lottie/Rive/GSAP plugin — the bundle rule (`web-performance`).
- *"Why does the 3D hero eat the battery / the 60fps becomes 20"?* → it's a 2D-engine trick or the un-culled scene → `threejs`'s render-loop rules (on-demand render, the frustum, the disposal), or it's CSS-3D where it should be a still image.
