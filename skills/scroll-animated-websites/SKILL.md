---
name: scroll-animated-websites
description: >
  Create cinematic, high-polish websites whose visuals, text, layout, and media animate in response
  to scrolling. Use for scroll storytelling, product launches, portfolio pages, interactive explainers,
  parallax scenes, pinned sections, scrubbed timelines, spatial transitions, scroll-linked video,
  sticky compositions, and high-end editorial or marketing experiences.
---

# Scroll Animated Websites

Build websites where scrolling is not merely navigation.

Scrolling should function as a **timeline controller** for composition, motion, narrative progression, and spatial transitions.

The target is not "elements fade in while scrolling."

The target is a deliberately choreographed visual experience where the user feels they are moving through a sequence of designed scenes.

Think:

```text
editorial storytelling
+
motion design
+
cinematography
+
layout systems
+
interaction design
+
frontend engineering
```

not:

```text
normal webpage
+
lots of fade-up animations
```

---

# When to Use This Skill

Use this skill when the user asks for:

- scroll-driven websites
- cinematic product pages
- Apple-like product storytelling
- scrollytelling
- interactive editorial pages
- pinned scenes
- parallax
- scroll-linked transitions
- timeline-based page choreography
- scroll-controlled video
- horizontal sections controlled by vertical scroll
- sticky storytelling layouts
- animated portfolios
- immersive landing pages
- interactive explainers
- storytelling microsites

Do not use heavy scroll animation automatically for ordinary documentation, dashboards, admin panels, forms, or content whose main goal is speed and readability.

---

# Core Principle

The browser's scroll position is a continuously changing input value.

Convert it into normalized progress:

```js
progress = 0 → 1
```

Then use that progress to drive visual state.

Conceptually:

```text
scroll position
      ↓
 normalized progress
      ↓
 scene timeline
      ↓
 transforms / opacity / clipping / video / text / camera
```

Do not think:

> "When element enters viewport, play animation."

Think:

> "At this part of the scroll timeline, what should the composition look like?"

That distinction produces much better results.

---

# Three Animation Models

There are three major classes of scroll animation.

## 1. Triggered Animation

An animation fires when an element enters a region.

Examples:

- fade in
- slide in
- stagger
- count-up
- line reveal

Use:

- IntersectionObserver
- CSS transitions
- GSAP triggers

Good for supporting motion.

Not sufficient for complex cinematic work.

---

## 2. Scrubbed Animation

Animation progress follows scroll progress continuously.

Example:

```text
scroll 30% → animation 30%
scroll 62% → animation 62%
scroll upward → animation reverses
```

Use for:

- object movement
- image scaling
- text transformations
- pinned scenes
- camera-like movement
- progress illustrations
- charts
- video scrubbing

This is the core technique for sophisticated scrollytelling.

---

## 3. Pinned / Sticky Scene

A visual composition remains fixed while the document continues scrolling.

During that scroll distance, the scene changes.

Example:

```text
user scrolls ↓

┌──────────────────────┐
│                      │
│   fixed product      │
│                      │
│   changing labels    │
│                      │
└──────────────────────┘

scene stays in place
timeline advances
```

This is one of the strongest techniques available.

---

# Preferred Technology

Choose the smallest tool that fits the choreography.

## Native CSS / Browser APIs

Prefer when possible:

```text
position: sticky
transform
opacity
clip-path
IntersectionObserver
CSS Scroll-Driven Animations
ViewTimeline
ScrollTimeline
```

Use native approaches for simpler experiences.

---

## GSAP + ScrollTrigger

Use when the experience requires:

- complex scrubbed timelines
- pinning
- multiple coordinated animations
- nested timelines
- callbacks
- precise sequencing
- excellent cross-browser behavior
- sophisticated motion control

Typical setup:

```js
gsap.registerPlugin(ScrollTrigger);
```

Then:

```js
gsap.to(".product", {
  y: -200,
  rotation: 8,
  scrollTrigger: {
    trigger: ".scene",
    start: "top top",
    end: "+=1500",
    scrub: true,
    pin: true
  }
});
```

---

# Recommended Architecture

```text
src/
├─ main.js
├─ styles.css
├─ motion/
│  ├─ scroll.js
│  ├─ timelines.js
│  ├─ parallax.js
│  ├─ reveals.js
│  └─ reduced-motion.js
├─ scenes/
│  ├─ hero.js
│  ├─ product.js
│  ├─ story.js
│  └─ finale.js
└─ utils/
   ├─ clamp.js
   ├─ lerp.js
   └─ viewport.js
```

Do not let all scroll logic accumulate into one giant anonymous callback.

---

# Scene-Based Thinking

Divide the page into scenes.

Example:

```text
Scene 1: Hero
Scene 2: Product enters
Scene 3: Product disassembles
Scene 4: Technical explanation
Scene 5: Reassembly
Scene 6: Final CTA
```

Each scene should have a clear visual purpose.

Do not animate individual DOM nodes without understanding which scene they belong to.

---

# Scroll Distance Is Time

Pinned scenes need enough physical scroll distance.

Example:

```css
.scene {
  min-height: 300vh;
}
```

or using GSAP:

```js
end: "+=2400"
```

Think of this as the duration of a movie shot.

Too little scroll distance:

- animation feels frantic
- information cannot be read
- motion jumps

Too much:

- scene feels stuck
- user becomes impatient

---

# Normalize Progress

Useful helper:

```js
function clamp(v, min = 0, max = 1) {
  return Math.max(min, Math.min(max, v));
}

function progressBetween(value, start, end) {
  return clamp((value - start) / (end - start));
}
```

Now one section can contain multiple subranges:

```text
0.00–0.20   title fades
0.15–0.40   object scales
0.35–0.65   diagram appears
0.60–0.85   copy changes
0.80–1.00   scene exits
```

This is far better than arbitrary scroll event conditions.

---

# Timeline Design

Plan timelines explicitly.

Example:

```text
0.00  product centered
0.10  hero copy fades
0.18  camera pushes in
0.30  casing separates
0.44  component labels appear
0.63  components rotate
0.78  labels disappear
0.86  parts reassemble
1.00  product exits upward
```

Then implement.

Do not discover choreography accidentally in code.

---

# Pinned Sections

Using CSS:

```css
.story {
  height: 300vh;
}

.story__stage {
  position: sticky;
  top: 0;
  height: 100vh;
  overflow: hidden;
}
```

Then derive progress based on the parent scroll distance.

This pattern is extremely powerful and requires no library.

---

# Native Sticky Progress

Example:

```js
function getStickyProgress(section) {
  const rect = section.getBoundingClientRect();
  const scrollable = section.offsetHeight - innerHeight;

  if (scrollable <= 0) return 0;

  return Math.max(
    0,
    Math.min(
      1,
      -rect.top / scrollable
    )
  );
}
```

Then:

```js
const p = getStickyProgress(section);

product.style.transform =
  `translate3d(0, ${-p * 300}px, 0)`;
```

---

# Never Animate Layout Properties Unless Necessary

Prefer:

```text
transform
opacity
filter
clip-path
```

Avoid frequently animating:

```text
top
left
width
height
margin
padding
```

These can cause layout recalculation.

Good:

```js
transform: translate3d(...)
```

Bad:

```js
top = ...
```

when transform can solve the same problem.

---

# GPU-Friendly Motion

Use:

```css
transform: translate3d(0, 0, 0);
will-change: transform;
```

sparingly.

Do not place `will-change` on dozens of elements permanently.

It consumes memory.

Apply it only to important moving elements.

---

# Parallax

Parallax means different layers move at different apparent speeds.

Example:

```js
const offset = scrollY;

background.style.transform =
  `translate3d(0, ${offset * 0.12}px, 0)`;

midground.style.transform =
  `translate3d(0, ${offset * 0.28}px, 0)`;

foreground.style.transform =
  `translate3d(0, ${offset * 0.5}px, 0)`;
```

Keep it restrained.

Excessive parallax feels like a template effect.

---

# Good Parallax

Use parallax to reinforce depth:

```text
far background    0.05×
background        0.10×
midground         0.25×
foreground        0.45×
very near object  0.65×
```

Do not make layers drift in unrelated directions without reason.

---

# Cinematic Scale

Scale is one of the strongest scroll effects.

Example:

```js
const scale = 1 + progress * 0.35;
```

Use it for:

- product push-ins
- image reveals
- simulated camera movement
- entering a scene
- zooming into details

Avoid scaling text excessively.

---

# Simulated Camera Movement

A webpage can mimic camera movement without 3D.

Use:

```text
translate
scale
crop
layer parallax
blur
opacity
```

Example:

```js
stage.style.transform = `
  translate3d(
    ${lerp(0, -80, p)}px,
    ${lerp(0, -40, p)}px,
    0
  )
  scale(${lerp(1, 1.18, p)})
`;
```

This creates a virtual camera pan-and-zoom.

---

# Clip Reveals

Useful for editorial transitions.

Example:

```css
.image {
  clip-path: inset(0 100% 0 0);
}
```

Animate toward:

```css
clip-path: inset(0 0 0 0);
```

Can also reveal vertically, diagonally, or with polygons.

Do not use complex clip paths on too many large elements.

---

# Masking

CSS masks are useful for sophisticated transitions.

Examples:

- gradient reveal
- texture reveal
- circular reveal
- light sweep
- image-to-image transition

Use when it supports the story.

Do not use masking merely because it looks advanced.

---

# Text Animation

Use text sparingly.

Good patterns:

- line-by-line reveal
- word stagger
- opacity crossfade
- vertical mask reveal
- character tracking change
- pinned heading with changing body copy

Avoid animating every individual letter unless the typography itself is the focus.

---

# Line Reveal Pattern

HTML:

```html
<div class="line-mask">
  <span class="line">Precision engineered.</span>
</div>
```

CSS:

```css
.line-mask {
  overflow: hidden;
}

.line {
  display: block;
}
```

Animate:

```js
gsap.fromTo(".line", {
  yPercent: 110
}, {
  yPercent: 0
});
```

GSAP establishes the start below the mask and returns the line to its normal baseline.

---

# Text Crossfades

For pinned stories, one visual can remain fixed while copy changes.

Example:

```text
product remains centered

copy 1 → fades out
copy 2 → fades in
copy 2 → fades out
copy 3 → fades in
```

This is often cleaner than moving the whole layout repeatedly.

---

# Horizontal Scroll From Vertical Scroll

Use carefully.

Pattern:

```text
vertical document movement
        ↓
horizontal scene translation
```

Example:

```js
const distance = () => Math.max(0, track.scrollWidth - innerWidth);

gsap.to(track, {
  x: () => -distance(),
  ease: "none",
  scrollTrigger: {
    trigger: section,
    start: "top top",
    end: () => `+=${Math.max(1, distance())}`,
    scrub: true,
    pin: true,
    invalidateOnRefresh: true
  }
});
```

Use for:

- timelines
- galleries
- process diagrams
- large landscapes

Do not hijack scrolling without clear visual logic.

---

# Image Sequence Animation

A highly cinematic technique is rendering an image sequence based on scroll progress.

Example:

```text
frame 0001
frame 0002
frame 0003
...
frame 0180
```

Then:

```js
const frame = Math.floor(progress * (frameCount - 1));
```

Draw that frame to canvas.

Use for:

- product rotation
- exploded views
- cinematic transitions
- high-end product storytelling

---

# Image Sequence Architecture

Prefer canvas:

```js
const canvas = document.querySelector("canvas");
const ctx = canvas.getContext("2d");
```

Then:

```js
ctx.drawImage(frames[currentFrame], 0, 0);
```

Do not put hundreds of `<img>` elements into the DOM.

---

# Preloading Frames

Do not begin playback before enough frames are ready.

Example:

```js
const frames = [];

for (let i = 0; i < FRAME_COUNT; i++) {
  const img = new Image();
  img.src = frameUrl(i);
  frames.push(img);
}
```

For production, add actual load tracking.

Use WebP or AVIF if quality is acceptable.

---

# Video Scrubbing

Alternative to image sequences.

Example:

```js
video.currentTime = progress * video.duration;
```

However, random seeking through compressed video can be expensive or inaccurate depending on encoding.

For smooth scroll scrubbing:

- encode with frequent keyframes
- keep duration reasonable
- consider image sequences for absolute precision
- test Safari carefully

---

# Sticky Media + Scrolling Copy

A common editorial pattern:

```text
┌──────────────┬───────────────┐
│              │ copy block 1  │
│ sticky media │               │
│              │ copy block 2  │
│              │               │
│              │ copy block 3  │
└──────────────┴───────────────┘
```

The left visual remains sticky while text scrolls.

Use IntersectionObserver to switch media states.

This often provides excellent usability with moderate technical complexity.

---

# Progress-Based State Changes

Do not continuously animate everything if discrete states are enough.

Example:

```js
if (progress < 0.33) {
  setMode("closed");
} else if (progress < 0.66) {
  setMode("open");
} else {
  setMode("exploded");
}
```

Combine discrete states with continuous transforms.

---

# Easing

Scrubbed scroll animation often uses:

```text
ease: none
```

because scrolling itself controls the timing.

Triggered animations can use easing.

Good defaults:

```text
power2.out
power3.out
expo.out
```

Avoid excessive spring or elastic easing in serious cinematic pages.

---

# Scroll Smoothing

Do not automatically install smooth-scroll libraries.

Native scrolling is usually more robust.

If custom smooth scrolling is needed:

- preserve accessibility
- preserve keyboard navigation
- keep browser history behavior intact
- test trackpads
- test touch
- test reduced motion
- avoid making scroll feel delayed

Bad smooth scrolling causes more UX damage than benefit.

---

# Lenis / Smooth Scroll Libraries

Can be useful for:

- continuous cinematic motion
- consistent interpolation
- synchronized animation systems

But use only when the experience benefits materially.

Do not introduce smooth scrolling merely because modern portfolio sites use it.

---

# Scroll Velocity

Velocity can affect effects such as skew or blur.

Example concept:

```js
const velocity = currentScroll - previousScroll;
```

Then:

```js
skew = clamp(velocity * 0.04, -6, 6);
```

Use lightly.

High velocity effects quickly become gimmicky.

---

# Direction-Aware Motion

Track:

```text
scrolling down
scrolling up
```

Potential uses:

- header hide/show
- directional transitions
- reversing subtle offsets
- changing navigation states

Do not make major narrative content depend on scroll direction unless necessary.

---

# Layered Scene Example

```text
Scene container — 400vh

Sticky viewport — 100vh
├─ background image
├─ gradient atmosphere
├─ product
├─ annotation layer
├─ heading
├─ copy
└─ progress indicator
```

Timeline:

```text
0.00–0.18
hero copy fades

0.12–0.35
product grows 1.0 → 1.25

0.28–0.52
background shifts left

0.42–0.64
annotation 1 appears

0.58–0.78
annotation 1 disappears
annotation 2 appears

0.76–1.00
product moves upward
next section emerges
```

Overlapping ranges create continuity.

---

# Do Not Sequence Everything Serially

Bad:

```text
A finishes
then B starts
then C starts
then D starts
```

This creates mechanical motion.

Better:

```text
A starts
B begins before A ends
C begins while B is moving
D begins near C's midpoint
```

Motion should overlap like cinematic editing.

---

# Spatial Continuity

Objects should feel like they occupy a coherent space across transitions.

If a product is center-screen in one scene, the next scene might:

- push into it
- rotate around it
- move labels around it
- reveal interior layers
- let it become the background

Do not constantly destroy one scene and spawn unrelated content from nowhere.

---

# Scroll Narrative

Each scene should answer:

```text
What is being introduced?
What changes?
Why does scrolling reveal this?
What should the user understand now?
```

If the animation does not improve understanding, remove it.

---

# Section Transitions

Strong transitions include:

- shared-object movement
- scale into next scene
- foreground wipe
- mask reveal
- color takeover
- camera pan
- depth push
- image continuation

Weak transitions:

- everything fades to black
- arbitrary wipe every section
- repeated fade-up cards

---

# Background Color Transitions

Smoothly changing page background can unify scenes.

Example:

```js
gsap.to(document.body, {
  backgroundColor: "#ece8df",
  scrollTrigger: {
    trigger: ".light-section",
    scrub: true
  }
});
```

Coordinate text color at the same time.

---

# WebGL

Use WebGL / Three.js only when necessary.

Good reasons:

- real 3D product
- depth-sensitive particles
- shaders
- 3D camera movement
- large interactive environments

Bad reason:

> "The website should look premium."

High-end scroll storytelling can be done entirely with DOM, SVG, Canvas, CSS, and GSAP.

---

# SVG

SVG is excellent for:

- diagrams
- paths
- line drawings
- charts
- technical illustrations
- path drawing animations

Example:

```css
path {
  stroke-dasharray: 800;
  stroke-dashoffset: 800;
}
```

Then animate dash offset toward `0`.

Useful for scroll-explained systems.

---

# SVG Path Following

An element can travel along a path.

Use for:

- manufacturing flows
- logistics
- process diagrams
- data movement
- educational explainers

The motion must reinforce meaning.

---

# Canvas

Canvas is useful when:

- many particles
- frame sequences
- procedural visuals
- simulation
- large animated backgrounds
- pixel effects

Do not use Canvas for content that needs semantic HTML.

---

# DOM vs Canvas vs WebGL

Choose deliberately.

```text
DOM:
text, layout, controls, semantic content

SVG:
diagrams, vectors, paths

Canvas:
dense 2D rendering, sequences, particles

WebGL:
true 3D, shaders, high-volume GPU effects
```

Hybrid pages are usually strongest.

---

# Responsive Strategy

Do not merely shrink desktop choreography.

Mobile often needs different motion.

Desktop:

```text
large pinned scenes
horizontal movement
multi-layer parallax
side-by-side compositions
```

Mobile:

```text
shorter pins
simpler transforms
vertical compositions
less parallax
more direct content flow
```

Use separate timelines where needed.

---

# MatchMedia with GSAP

Example:

```js
const mm = gsap.matchMedia();

mm.add("(min-width: 900px)", () => {
  // desktop timeline
});

mm.add("(max-width: 899px)", () => {
  // mobile timeline
});
```

Do not force one timeline to serve every viewport.

---

# Resize Handling

Scroll calculations can break after resizing.

Recalculate:

- viewport height
- scene sizes
- canvas dimensions
- image fitting
- ScrollTrigger positions

For GSAP:

```js
ScrollTrigger.refresh();
```

Use intelligently, not continuously.

---

# Mobile Safari

Test explicitly.

Potential issues:

- dynamic browser chrome changes viewport height
- autoplay restrictions
- sticky quirks
- expensive filters
- large fixed backgrounds
- video decoding
- memory pressure

Prefer:

```css
height: 100svh;
```

or appropriate `dvh` / `lvh` units depending on intent.

---

# Accessibility

Scroll animation must not trap the user.

Always preserve:

- normal scrolling
- keyboard navigation
- semantic document order
- readable text
- working links
- focus behavior

Never require precise scrolling to access critical information.

---

# Reduced Motion

Respect:

```css
@media (prefers-reduced-motion: reduce)
```

In reduced-motion mode:

- remove parallax
- remove large scale transitions
- disable scroll scrubbing where appropriate
- show final visual states directly
- keep essential content visible

Example:

```js
const reduceMotion =
  matchMedia("(prefers-reduced-motion: reduce)").matches;
```

Then build a simpler experience.

---

# Avoid Motion Sickness

Large simultaneous movement can cause discomfort.

Be cautious with:

- huge zooms
- strong background parallax
- rapid rotational motion
- motion blur
- camera banking
- scroll-jacking
- viewport-filling horizontal movement

The user's hand is moving vertically.
The page should not constantly contradict that motion.

---

# Performance

Target smooth interaction.

Avoid:

- heavy layout recalculation
- dozens of simultaneous filters
- huge unoptimized images
- oversized videos
- many fixed layers
- JS work on every scroll event without throttling
- unnecessary DOM reads and writes interleaved

---

# Scroll Event Pattern

Avoid:

```js
window.addEventListener("scroll", () => {
  // huge workload
});
```

Prefer requestAnimationFrame scheduling:

```js
let ticking = false;

window.addEventListener("scroll", () => {
  if (!ticking) {
    requestAnimationFrame(() => {
      updateScene();
      ticking = false;
    });

    ticking = true;
  }
});
```

Libraries like GSAP already solve much of this.

---

# Separate Reads and Writes

Bad:

```js
element.style.transform = ...
const rect = element.getBoundingClientRect();
other.style.opacity = ...
```

Better:

```text
1. read geometry
2. calculate
3. write styles
```

Avoid forced synchronous layouts.

---

# Asset Optimization

Use:

- AVIF / WebP
- responsive `srcset`
- lazy loading
- compressed video
- low-resolution placeholders
- preloading only for imminent scenes

Do not load the entire experience at once if it contains many large assets.

---

# Hero Preload

Critical first-scene assets should preload.

Example:

```html
<link
  rel="preload"
  href="/hero.avif"
  as="image"
/>
```

Do not preload every image on the website.

---

# Progressive Loading

For long narrative pages:

```text
load hero immediately
load scene 2 soon
load scene 3 when scene 1 is active
load scene 4 later
```

This improves startup time substantially.

---

# Scroll Progress Indicator

Useful for long stories.

Examples:

- thin vertical line
- chapter marker
- subtle horizontal bar
- section dots

Keep it subordinate.

Avoid giant progress UI competing with content.

---

# Navigation

Long scrollytelling pages still need navigation.

Possible:

```text
Overview
Design
Mechanism
Performance
Details
```

Clicking can jump to scene anchors.

Ensure history and back-button behavior remain sane.

---

# Deep Links

Important sections should have IDs.

Example:

```html
<section id="performance">
```

A cinematic website is still a website.

Allow users and search engines to identify sections.

---

# SEO

Do not hide all meaningful content inside Canvas.

Use semantic HTML for:

- headings
- paragraphs
- links
- metadata
- product information

Visual animation may duplicate or enhance semantic content.

Search engines and assistive technologies should still understand the page.

---

# Content First

Before building animation, identify:

```text
message
hierarchy
story order
visual emphasis
```

Do not use motion to compensate for vague content.

---

# Visual Hierarchy

At any scroll position, the user should know what matters.

Use:

- scale
- contrast
- position
- empty space
- light
- depth
- focus
- motion

Motion is only one hierarchy tool.

---

# Focal Motion

A useful rule:

At any moment, only one or two things should be doing visually dominant movement.

Too many independent movements destroy focus.

---

# Scroll Choreography Workflow

Follow this process.

## Phase 1 — Story

Write scene sequence:

```text
scene
purpose
visual
copy
transition
```

Example:

```text
Hero
introduce product
large centered render
one sentence
push into product
```

---

## Phase 2 — Static Layout

Build every scene without animation.

Check:

- spacing
- typography
- responsive behavior
- image quality
- visual hierarchy

The page should remain understandable with JavaScript disabled where practical.

---

## Phase 3 — Motion Map

For every scene specify:

```text
trigger
start
end
pin?
animated properties
progress ranges
transition to next scene
```

---

## Phase 4 — Primary Motion

Implement only major choreography:

- pinning
- major transforms
- scene transitions
- product movement

Ignore decorative animation.

---

## Phase 5 — Supporting Motion

Add:

- text reveals
- small parallax
- annotations
- subtle opacity shifts

---

## Phase 6 — Polish

Add:

- micro-interactions
- refined easing
- responsive variants
- loading behavior
- accessibility
- reduced motion

---

## Phase 7 — QA

Test:

```text
slow scrolling
fast scrolling
scroll reversal
trackpad
mouse wheel
touch
keyboard
resize
mobile Safari
reduced motion
low-power device
```

---

# Anti-Slop Rules

Do not produce stereotypical "AI premium website" motion.

Avoid:

- everything fading upward
- giant gradient blobs drifting behind every section
- excessive glassmorphism
- every heading splitting into letters
- endless blur transitions
- mouse-following blobs
- constant cursor gimmicks
- enormous headline + rotating 3D object without narrative
- scroll-jacking
- gratuitous sideways sections
- random WebGL particles
- dozens of easing styles
- every section pinned
- motion with no semantic purpose

---

# Common Failure Modes

## Too much pinning

If every section pins, the page feels trapped.

Use pins only for scenes that genuinely need time.

---

## Too little scroll distance

Animation becomes frantic.

Increase scene duration or reduce animation complexity.

---

## Too much scroll distance

Users feel stuck.

Shorten the pin or increase visual progression.

---

## Inconsistent motion direction

Objects move left, right, up, down without spatial reason.

Define a directional language.

---

## Copy unreadable during motion

If text is moving while users need to read it, stop or slow the motion.

---

## Desktop-only thinking

A desktop masterpiece can become unusable on mobile.

Design mobile separately.

---

## Excessive blur

Blur is expensive and visually muddy.

Use sparingly.

---

## Heavy content shifts

Scroll animation should not cause layout instability.

Reserve space ahead of time.

---

# Quality Bar

A strong scroll-animated website should satisfy most of these:

## Story

- each scene has a clear purpose
- scrolling reveals information logically
- scene order feels inevitable
- transitions maintain continuity

## Motion

- major motion is scrubbed cleanly
- triggered motion is restrained
- animation reverses properly when scrolling upward
- no jarring jumps
- no arbitrary movement

## Visual

- strong static layouts
- intentional focal points
- clear depth
- consistent visual language
- motion reinforces hierarchy

## Engineering

- no major scroll jank
- animations use performant properties
- mobile has its own treatment where needed
- assets load progressively
- resize behavior is stable

## Accessibility

- normal document flow remains meaningful
- reduced motion works
- keyboard navigation works
- content does not disappear permanently
- scrolling is never trapped

---

# Final Evaluation

Before considering the page finished, test these questions.

### 1. Freeze all animation.

Does the page still look well designed?

If no, fix the layout.

### 2. Scroll very slowly.

Does every transition remain coherent?

If no, the timeline is poorly constructed.

### 3. Scroll extremely fast.

Does the page recover cleanly without broken states?

If no, state management is fragile.

### 4. Scroll backward.

Do animations reverse naturally?

If no, the implementation is too event-driven.

### 5. Disable motion.

Can the user still understand everything?

If no, motion is carrying content that should exist semantically.

### 6. Use a phone.

Does the experience remain readable and usable?

If no, create a separate mobile choreography.

---

# Mental Model

Treat the page as a sequence of spatial scenes.

The user's scroll wheel is the playhead.

The DOM is the stage.

CSS transforms are the camera.

Sticky positioning provides time.

GSAP or native scroll timelines provide choreography.

Images, SVG, Canvas, video, and WebGL are visual actors.

The goal is not to impress the user with animation.

The goal is to make **scrolling itself feel like moving through the story**.
