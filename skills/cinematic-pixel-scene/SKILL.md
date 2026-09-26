---
name: cinematic-pixel-scene
description: >
  Create highly detailed, atmospheric, interactive pixel-art web pages rendered in JavaScript,
  including layered scene composition, procedural animation, lighting, weather, ambience,
  environmental sound, and responsive presentation. Use when building pages inspired by
  cinematic pixel scenes, animated dioramas, quiet-game-like environments, lo-fi interactive
  worlds, or richly illustrated Canvas experiences.
---

# Cinematic Pixel Scene

Build web pages that feel like living pixel-art environments rather than conventional websites.

The target is not "retro UI." The target is a **small, cinematic world**: carefully composed architecture, restrained motion, coherent lighting, environmental sound, spatial depth, and precise pixel rendering.

A good result should feel closer to a tiny game scene or animated illustration than to a webpage with decorative pixel graphics.

---

## Core Principle

Do not manually paint every screen pixel.

Represent the scene as a hierarchy of reusable drawing operations:

```text
World
├─ background
├─ distant environment
├─ architecture
├─ props
├─ characters
├─ vehicles / moving objects
├─ lighting
├─ weather / particles
├─ reflections
├─ foreground occlusion
└─ post-processing
```

Then redraw the scene every animation frame.

The browser acts as a lightweight 2D game engine.

---

# When to Use This Skill

Use this skill when the user asks for:

- cinematic pixel art
- animated pixel scenes
- atmospheric landing pages
- interactive environmental scenes
- train stations, streets, cafés, apartments, factories, rain scenes, night scenes, etc.
- ambient soundscapes synchronized to visuals
- procedural pixel art
- JavaScript-rendered illustrations
- game-like web scenes without requiring a full game engine
- highly detailed Canvas artwork
- quiet / meditative / lo-fi interactive websites

Do not use this skill for ordinary dashboard UI, icon work, conventional responsive websites, or static pixel-art images unless the scene itself is the primary experience.

---

# Preferred Technical Architecture

Default to:

```text
HTML
CSS
JavaScript
Canvas 2D
Web Audio API
requestAnimationFrame()
```

Avoid WebGL unless the requested effects actually need it.

Canvas 2D is sufficient for most scenes and gives better control over pixel alignment.

Suggested project structure:

```text
src/
├─ main.js
├─ config.js
├─ palette.js
├─ state.js
├─ renderer.js
│
├─ scene/
│  ├─ background.js
│  ├─ architecture.js
│  ├─ platform.js
│  ├─ props.js
│  ├─ characters.js
│  ├─ vehicles.js
│  ├─ lighting.js
│  ├─ weather.js
│  ├─ reflections.js
│  └─ foreground.js
│
├─ systems/
│  ├─ animation.js
│  ├─ particles.js
│  ├─ timeline.js
│  ├─ camera.js
│  ├─ audio.js
│  └─ interaction.js
│
└─ utils/
   ├─ draw.js
   ├─ random.js
   ├─ math.js
   └─ color.js
```

For small experiments, one file is acceptable, but keep conceptual separation inside the code.

---

# Logical Resolution

Render at a deliberately low internal resolution.

Typical examples:

```js
const VIEW_W = 480;
const VIEW_H = 270;
```

or:

```js
const VIEW_W = 640;
const VIEW_H = 360;
```

Then scale the canvas visually with CSS.

```css
canvas {
  width: 100vw;
  height: 100vh;
  image-rendering: pixelated;
}
```

Always disable smoothing:

```js
ctx.imageSmoothingEnabled = false;
```

The logical resolution should be low enough that each pixel matters but high enough to carry architectural detail.

Good ranges:

```text
320×180   very chunky
480×270   strong default
640×360   highly detailed
800×450   near-illustrative
```

Do not render directly at device resolution unless deliberately creating fine-grained pixel art.

---

# Pixel Discipline

Pixel art depends on grid consistency.

Prefer integer coordinates:

```js
ctx.fillRect(104, 52, 18, 6);
```

Avoid:

```js
ctx.fillRect(104.38, 51.72, 18.42, 6.19);
```

Create helpers:

```js
const px = Math.round;

function rect(x, y, w, h, color) {
  ctx.fillStyle = color;
  ctx.fillRect(
    px(x),
    px(y),
    px(w),
    px(h)
  );
}
```

All scene geometry should eventually land on the logical pixel grid.

Fractional coordinates are acceptable internally for motion, but round at render time.

---

# Scene Construction Strategy

Do not begin with animation.

Build in this order:

1. composition
2. value structure
3. architecture
4. color palette
5. props
6. characters
7. lighting
8. atmospheric depth
9. animation
10. sound
11. interactions
12. polish

A weak static frame cannot be rescued by animation.

The scene should already look compelling as a screenshot before movement is added.

---

# Composition

Treat the scene as a cinematography problem.

Decide first:

- horizon
- focal point
- foreground / midground / background
- brightest area
- darkest area
- dominant lines
- empty-space balance
- visual entry point
- where the eye should travel

Useful composition patterns:

```text
Foreground framing
      ↓
[dark column]     [dark object]

       central lit subject

------------ platform ------------
----------- rail line ------------
```

Use architecture to create leading lines.

Examples:

- tracks converge toward a vanishing point
- awnings point toward the subject
- lights form perspective rhythm
- poles create framing
- windows repeat horizontally
- foreground objects partially obscure the scene

Do not distribute visual detail evenly.

Important areas deserve high detail.
Peripheral areas should often remain simpler.

---

# Layering

Render back-to-front.

Recommended order:

```js
drawSky();
drawDistantSilhouettes();
drawAtmosphereBack();
drawArchitecture();
drawInteriorSpaces();
drawStaticProps();
drawGround();
drawRailsOrRoad();
drawCharactersBack();
drawVehicles();
drawCharactersFront();
drawLocalLights();
drawReflections();
drawWeather();
drawForegroundObjects();
drawAtmosphereFront();
drawPostEffects();
```

Correct layering is one of the main sources of perceived depth.

---

# Reusable Drawing Primitives

Build helpers before building the scene.

Examples:

```js
rect()
line()
pixel()
cluster()
shadowRect()
outlineRect()
window()
lamp()
sign()
beam()
pillar()
bench()
door()
person()
reflection()
```

Example:

```js
function drawWindow(x, y, w, h, opts = {}) {
  rect(x, y, w, h, opts.frame ?? COLORS.frame);
  rect(x + 1, y + 1, w - 2, h - 2, opts.glass ?? COLORS.glass);

  if (opts.glow) {
    rect(x + 2, y + 2, w - 4, h - 4, opts.glow);
  }

  if (opts.divider) {
    rect(x + Math.floor(w / 2), y + 1, 1, h - 2, COLORS.frameDark);
  }
}
```

Complex objects should be compositions of primitives, not giant procedural functions containing hundreds of unrelated operations.

---

# Palette Design

Do not use arbitrary colors everywhere.

Create a compact palette.

Example:

```js
export const COLORS = {
  black: "#080a0d",
  deepShadow: "#11151b",
  shadow: "#1b232b",
  steelDark: "#27323a",
  steel: "#36454f",
  steelLight: "#566a76",

  warmDark: "#7c3f27",
  warm: "#c46b36",
  warmBright: "#f0a45d",
  warmHot: "#ffd18b",

  coolDark: "#243b58",
  cool: "#3d6080",
  coolBright: "#74a3c7",

  skinShadow: "#7b4c3d",
  skin: "#b8775e",
  skinLight: "#d89a79"
};
```

Reuse the same colors across related surfaces.

Lighting coherence matters more than color count.

A lamp's warm orange should appear in:

- the lamp
- nearby wall
- character rim light
- wet ground
- metal rail
- fog
- reflected highlights

That repeated palette makes the light source believable.

---

# Value Before Color

The image must work in grayscale.

Ask:

```text
Where is the darkest mass?
Where is the brightest region?
Can the focal point be identified without color?
```

A cinematic scene typically uses:

- large dark masses
- limited midtones
- small areas of strong highlights

Do not make every object equally bright.

---

# Architecture

Architecture creates credibility.

Prioritize:

- consistent perspective
- repeated structural spacing
- believable dimensions
- columns
- beams
- windows
- doors
- signage
- trim
- pipes
- cables
- vents
- roof geometry
- floor divisions
- rail / curb / platform edges

Use repetition with controlled variation.

Example:

```js
for (let i = 0; i < 6; i++) {
  drawPillar(80 + i * 54, 42);
}
```

Then break repetition deliberately:

```js
drawPoster(188, 91);
drawBrokenLamp(296, 48);
drawVendingMachine(338, 105);
```

Perfect repetition looks synthetic.
Controlled irregularity looks inhabited.

---

# Detail Hierarchy

Use three levels.

## Macro

Large shapes:

- station
- wall
- road
- building
- train
- sky

## Meso

Medium details:

- windows
- benches
- pillars
- vending machines
- signs
- doors

## Micro

Tiny detail:

- rivets
- rain streaks
- cable clips
- tiny labels
- reflected pixels
- scratches
- individual light points

Do not start with micro-detail.

Macro and meso geometry create almost all structural believability.

---

# Characters

Pixel characters should remain simple.

Use strong silhouettes before facial detail.

A small person can be constructed from:

```text
head
hair
torso
arms
legs
shoes
shadow
rim light
```

Example:

```js
function drawPerson(x, y, p) {
  rect(x + 2, y, 4, 4, p.hair);
  rect(x + 2, y + 2, 4, 3, p.skin);

  rect(x + 1, y + 5, 6, 9, p.coat);
  rect(x, y + 7, 2, 7, p.coatDark);
  rect(x + 7, y + 7, 2, 7, p.coatDark);

  rect(x + 2, y + 14, 2, 7, p.pants);
  rect(x + 5, y + 14, 2, 7, p.pants);

  rect(x + 1, y + 21, 3, 1, p.shoe);
  rect(x + 5, y + 21, 3, 1, p.shoe);
}
```

Identity should come from:

- posture
- clothing shape
- hair
- bag
- hat
- stance
- silhouette

not from attempting to render detailed faces at tiny scale.

---

# Restrained Animation

Do not animate everything.

A cinematic scene is usually mostly still.

Typical ratio:

```text
90–97% visually static
3–10% changing
```

Good motion:

- rain
- fog
- steam
- distant signage
- train movement
- very subtle character breathing
- blinking
- coat movement
- flickering lights
- drifting leaves
- reflections
- passing silhouettes

Bad motion:

- every sign pulsing
- every light flickering
- characters constantly moving
- exaggerated parallax
- excessive camera motion

Stillness makes small motion meaningful.

---

# Animation Loop

Use:

```js
let last = performance.now();

function frame(now) {
  const dt = Math.min((now - last) / 1000, 0.05);
  last = now;

  update(dt, now);
  render(now);

  requestAnimationFrame(frame);
}

requestAnimationFrame(frame);
```

Cap large `dt` spikes.

Do not tie movement directly to frame count.

Bad:

```js
x += 1;
```

Good:

```js
x += speed * dt;
```

---

# Stable Randomness

Never regenerate random visual properties every frame.

Bad:

```js
const brightness = Math.random();
```

inside the render loop.

It creates flicker.

Instead create objects once:

```js
const drops = Array.from({ length: 120 }, () => ({
  x: rand(0, VIEW_W),
  y: rand(0, VIEW_H),
  speed: rand(40, 90),
  length: randInt(2, 5),
  alpha: rand(0.2, 0.7)
}));
```

Then update them over time.

For reproducible scenes, use seeded random generation.

Example:

```js
function mulberry32(seed) {
  return function () {
    let t = seed += 0x6D2B79F5;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
```

---

# Weather Systems

Particles are cheap and visually powerful.

## Rain

Each drop:

```js
{
  x,
  y,
  speed,
  length,
  alpha
}
```

Render:

```js
rect(
  drop.x,
  drop.y,
  1,
  drop.length,
  `rgba(190,210,225,${drop.alpha})`
);
```

Use multiple depth bands:

```text
background rain
midground rain
foreground rain
```

Foreground drops should be:

- faster
- brighter
- longer

Background drops should be:

- slower
- dimmer
- shorter

---

# Fog

Fog should drift slowly.

Avoid giant transparent white rectangles.

Use subtle patches and gradients when appropriate.

For strict pixel art, render low-frequency clusters:

```js
ctx.save();
for (const puff of fog) {
  ctx.globalAlpha = puff.alpha;
  rect(puff.x, puff.y, puff.w, puff.h, COLORS.fog);
}
ctx.restore();
```

Move by fractions internally and round while drawing.

---

# Steam and Smoke

Use expanding drifting particles:

```js
particle.y -= particle.speed * dt;
particle.x += Math.sin(time * particle.phase) * 3 * dt;
particle.alpha -= particle.fade * dt;
```

Keep opacity low.

---

# Lighting

Lighting is responsible for much of the cinematic quality.

Do not try to implement physically accurate global illumination.

Use artistic local lighting.

For each major light source, define:

```js
{
  x,
  y,
  color,
  radius,
  strength
}
```

Then deliberately place matching colors on nearby objects.

Think in terms of painted light rather than simulated light.

Example:

```text
lamp
 ↓
wall highlight
 ↓
character shoulder
 ↓
rail glint
 ↓
floor reflection
```

Those connected cues cause the viewer to infer one coherent light source.

---

# Rim Lighting

A tiny 1-pixel highlight can strongly separate silhouettes.

Example:

```js
rect(personX + 6, personY + 5, 1, 9, COLORS.warmBright);
```

Use only on the side facing the light source.

Do not outline every object with bright pixels.

---

# Glow

Prefer painted glow over blur filters.

For example:

```js
ctx.globalAlpha = 0.12;
rect(x - 4, y - 2, w + 8, h + 4, warmDark);

ctx.globalAlpha = 0.18;
rect(x - 2, y - 1, w + 4, h + 2, warm);

ctx.globalAlpha = 1;
rect(x, y, w, h, warmBright);
```

This preserves pixel sharpness.

Use CSS or Canvas blur only when the intended style allows softer effects.

---

# Reflections

Reflections do not need real ray tracing.

For wet ground:

1. repeat important bright shapes below a baseline
2. flip vertically when useful
3. reduce opacity
4. vertically compress
5. break them into horizontal fragments
6. slightly distort them

Example idea:

```js
function drawReflection(drawFn, baseline, alpha = 0.18) {
  ctx.save();

  ctx.globalAlpha = alpha;
  const verticalScale = 0.55;
  ctx.translate(0, baseline * (1 + verticalScale));
  ctx.scale(1, -verticalScale);

  drawFn();

  ctx.restore();
}
```

Then overlay ground-colored horizontal strips to break the reflection.

Reflections should be incomplete.

A perfect mirror looks wrong for pavement.

---

# Perspective

Even 2D pixel art needs coherent perspective.

Define a horizon:

```js
const HORIZON_Y = 92;
```

For basic perspective scaling:

```js
function depthScale(y) {
  return 0.45 + 0.55 * ((y - HORIZON_Y) / (VIEW_H - HORIZON_Y));
}
```

Objects further away can become:

- smaller
- less contrasty
- cooler
- less detailed
- more obscured by atmosphere

Do not overcomplicate this with full 3D mathematics unless needed.

---

# Atmospheric Perspective

Depth can be faked cheaply.

Far objects:

```text
lower contrast
lower saturation
less detail
closer to background hue
more haze
```

Near objects:

```text
higher contrast
sharper edges
stronger shadows
more texture
```

This is often more important than actual geometric perspective.

---

# Foreground Occlusion

Use dark nearby objects to frame the scene.

Examples:

- pillar
- railing
- doorway edge
- tree branch
- roof beam
- passing silhouette

Foreground shapes can cover 5–20% of the screen.

This makes the world feel larger than the viewport.

---

# Camera

Default to a fixed camera.

The scene should not feel like a website carousel.

If camera motion is used, keep it subtle:

- 1–4 px drift
- slow parallax
- user-controlled pan
- tiny movement on major events

Avoid constant floating-camera effects unless explicitly requested.

---

# Parallax

If needed, separate layers:

```js
const cameraX = ...;

drawBackground(-cameraX * 0.1);
drawMidground(-cameraX * 0.4);
drawForeground(-cameraX * 0.8);
```

Keep parallax subtle.

Large parallax distances easily destroy pixel-art coherence.

---

# Timeline Events

Scenes become much more convincing when occasional events occur.

Examples:

```text
00:00 ambience
00:07 light flickers once
00:18 distant train passes
00:31 character checks phone
00:49 announcement plays
01:04 train arrives
01:29 doors close
01:36 train leaves
```

Use probabilistic scheduling where appropriate.

Example:

```js
if (time > nextAnnouncement) {
  playAnnouncement();
  nextAnnouncement = time + rand(35, 80);
}
```

Do not make events fire continuously.

Rarity increases impact.

---

# Audio

Sound is part of the scene, not an afterthought.

Use Web Audio API for anything beyond a single loop.

Recommended structure:

```text
AudioContext
├─ ambience gain
├─ weather gain
├─ mechanical gain
├─ events gain
└─ master gain
```

Example:

```js
const audioCtx = new AudioContext();

const master = audioCtx.createGain();
master.gain.value = 0.7;
master.connect(audioCtx.destination);
```

---

# Browser Autoplay Rules

Browsers usually block audio until user interaction.

Provide an unobtrusive start interaction:

```text
Click to enter
Enable sound
Begin
```

Then:

```js
await audioCtx.resume();
```

Never attempt to evade browser autoplay policies.

---

# Audio Layers

Good environmental sound often uses multiple quiet layers.

Example:

```text
base ambience
+
wind
+
rain
+
electrical hum
+
distant city
+
rare announcements
+
vehicle events
```

The goal is not loudness.

The goal is environmental coherence.

---

# Audio Mixing

Avoid playing all sounds at full volume.

Typical relative levels:

```text
master ambience       0.35
rain                  0.25
mechanical hum        0.10
distant traffic       0.08
announcement          0.35
train arrival         0.45
UI sound              0.12
```

Exact numbers vary.

Ambient layers should usually sit below conscious attention.

---

# Spatial Audio

For moving objects, stereo positioning adds immersion.

Example:

```js
const pan = audioCtx.createStereoPanner();

pan.pan.value = Math.max(
  -1,
  Math.min(1, (train.x / VIEW_W) * 2 - 1)
);
```

Volume can depend on distance:

```js
const distance = Math.abs(train.x - listenerX);

gain.gain.value = 1 / (1 + distance * 0.02);
```

Do not attempt accurate acoustic simulation unless necessary.

Simple panning and attenuation are usually enough.

---

# Audio Asset Strategy

Possible sources:

- original recordings
- legally licensed ambience
- generated audio
- synthesized noise
- Web Audio oscillators and filters

Do not embed copyrighted commercial music without permission.

Prefer loops that can repeat seamlessly.

---

# Procedural Ambient Audio

Some sounds can be synthesized.

Example:

```js
const buffer = audioCtx.createBuffer(
  1,
  audioCtx.sampleRate * 2,
  audioCtx.sampleRate
);

const data = buffer.getChannelData(0);

for (let i = 0; i < data.length; i++) {
  data[i] = Math.random() * 2 - 1;
}
```

Then filter the noise for:

- wind
- rain texture
- ventilation hum

Do not rely on raw white noise without filtering.

---

# Interaction

Interactions should feel diegetic.

Good interactions:

- toggle sound
- click a light switch
- inspect a sign
- move camera slightly
- trigger train arrival
- open a door
- change weather
- advance time
- hover to reveal tiny environmental details

Avoid conventional giant floating controls over the scene.

Prefer small edge controls or in-world interactions.

---

# Responsive Presentation

Preserve the scene's aspect ratio.

Example:

```js
function resize() {
  const scale = Math.min(
    innerWidth / VIEW_W,
    innerHeight / VIEW_H
  );

  canvas.style.width = `${Math.floor(VIEW_W * scale)}px`;
  canvas.style.height = `${Math.floor(VIEW_H * scale)}px`;
}
```

Center the canvas.

Use letterboxing where necessary.

Do not stretch the artwork non-uniformly.

---

# Performance

Canvas 2D at a low logical resolution is usually inexpensive.

Still:

- avoid creating thousands of objects every frame
- reuse particle arrays
- avoid excessive `save()` / `restore()` nesting
- avoid expensive filters
- cache static complex layers when helpful
- avoid unnecessary DOM elements
- avoid allocating temporary arrays inside hot loops

Static layers can be prerendered to offscreen canvases:

```js
const architectureLayer = document.createElement("canvas");
```

Then each frame:

```js
ctx.drawImage(architectureLayer, 0, 0);
```

This is useful for highly complex static scenes.

---

# Offscreen Layer Strategy

A strong architecture:

```text
static background canvas
static architecture canvas
dynamic actors canvas
dynamic weather canvas
main composite canvas
```

Redraw static layers only when something changes.

This is optional at low resolutions but useful for large scenes.

---

# State Model

Keep visual state explicit.

Example:

```js
const state = {
  time: 0,

  weather: {
    rain: 0.8,
    fog: 0.3
  },

  train: {
    x: -120,
    state: "approaching"
  },

  lights: {
    platform: true,
    vending: true
  },

  audio: {
    enabled: false
  }
};
```

Do not scatter unrelated global variables throughout the file.

---

# Draw Functions Must Be Deterministic

Given the same state, rendering should produce the same frame.

Prefer:

```js
render(state)
```

over render functions that mutate world state.

Keep:

```text
update()
```

and:

```text
render()
```

conceptually separate.

---

# Scene Data

Separate content from renderer logic where practical.

Example:

```js
const lamps = [
  { x: 72, y: 42, type: "warm" },
  { x: 148, y: 42, type: "warm" },
  { x: 224, y: 42, type: "broken" }
];
```

Then:

```js
for (const lamp of lamps) {
  drawLamp(lamp);
}
```

This makes AI-driven iteration much easier.

---

# LLM Workflow

When creating a scene, do not attempt the final version in one pass.

Follow this loop:

```text
1. establish canvas and scaling
2. establish composition
3. render large silhouettes
4. inspect
5. add architecture
6. inspect
7. add palette and lighting
8. inspect
9. add props
10. inspect
11. add characters
12. inspect
13. add atmospheric effects
14. inspect
15. animate
16. inspect
17. add audio
18. inspect
19. optimize
20. final QA
```

Each pass should solve one class of problem.

---

# Visual Inspection

When tools permit screenshots, inspect the result visually after meaningful changes.

Do not assume code correctness implies visual correctness.

Check:

```text
Does perspective read correctly?
Is the scene too evenly detailed?
Is the focal point obvious?
Are lights coherent?
Are silhouettes readable?
Are pixel sizes consistent?
Is the frame too noisy?
Are foreground and background distinct?
Do repeated structures look artificial?
Are reflections too perfect?
```

---

# Reference-Driven Work

If the user provides references:

Extract:

```text
composition
palette
lighting direction
density
scale
architecture
mood
weather
camera position
animation restraint
```

Do not blindly copy the original artwork.

Translate its visual logic into a new scene.

---

# Anti-Slop Rules

Avoid common AI-generated scene problems.

## Do not:

- fill every empty area with random props
- use random neon colors
- make all lights glow equally
- overuse gradients
- animate everything
- make every object perfectly symmetrical
- scatter meaningless micro-details
- use inconsistent pixel sizes
- mix vector-smooth edges with chunky pixel geometry accidentally
- make signs unreadable blobs when text is important
- use perspective inconsistently
- place objects without considering scale
- create physically impossible architecture
- rely on blur to create atmosphere
- use giant particle counts to fake detail
- use full-strength sound loops simultaneously
- create constant repetitive sound events
- put conventional SaaS UI panels over the artwork unless requested

---

# Intentional Imperfection

Real environments are irregular.

Add:

- one broken light
- slight wall discoloration
- uneven puddles
- asymmetrical clutter
- worn signage
- different window brightness
- one tilted poster
- small debris clusters
- irregular roof details

Do not randomize everything.

Each imperfection should appear plausible.

---

# Pixel Text

Use pixel fonts only where appropriate.

For tiny environmental signs, custom bitmap lettering may look better.

If using canvas text:

```js
ctx.font = "8px 'PixelFont'";
ctx.textBaseline = "top";
```

Avoid browser anti-aliasing mismatch when text must visually merge with the scene.

For important signs, consider drawing letters manually or rendering into a tiny offscreen surface.

---

# Typography Outside the Scene

If the page contains explanatory text or controls, keep it subordinate.

Recommended:

- minimal typography
- neutral sans-serif or monospaced type
- small controls
- low visual contrast
- edges of viewport
- hideable overlays

The scene remains primary.

---

# Accessibility

Atmospheric experiences still need accessibility.

Include:

- sound toggle
- reduced-motion support
- keyboard controls when interactive
- sufficient control contrast
- semantic labels for controls
- descriptive fallback text
- no essential information conveyed only through audio

Respect:

```css
@media (prefers-reduced-motion: reduce)
```

In reduced-motion mode:

- reduce particles
- stop camera drift
- reduce flicker
- slow or freeze decorative movement

---

# Sound Accessibility

Never force audio.

Provide:

```text
Sound: On / Off
```

Remember user preference locally when appropriate.

Announcements containing important information should have text equivalents.

---

# Fullscreen

Immersive scenes often benefit from fullscreen.

Example:

```js
async function enterFullscreen() {
  await document.documentElement.requestFullscreen();
}
```

Fullscreen must be optional.

---

# Example Minimal Renderer

```js
const canvas = document.querySelector("canvas");
const ctx = canvas.getContext("2d");

const W = 480;
const H = 270;

canvas.width = W;
canvas.height = H;

ctx.imageSmoothingEnabled = false;

const state = {
  t: 0,
  rain: []
};

function rect(x, y, w, h, color) {
  ctx.fillStyle = color;
  ctx.fillRect(
    Math.round(x),
    Math.round(y),
    Math.round(w),
    Math.round(h)
  );
}

function init() {
  for (let i = 0; i < 90; i++) {
    state.rain.push({
      x: Math.random() * W,
      y: Math.random() * H,
      speed: 40 + Math.random() * 50,
      length: 2 + Math.floor(Math.random() * 4)
    });
  }
}

function update(dt) {
  state.t += dt;

  for (const drop of state.rain) {
    drop.y += drop.speed * dt;

    if (drop.y > H) {
      drop.y = -drop.length;
      drop.x = Math.random() * W;
    }
  }
}

function drawScene() {
  rect(0, 0, W, H, "#0b1015");

  // skyline
  rect(0, 80, W, 80, "#121b24");

  // platform
  rect(0, 160, W, 110, "#1a2024");

  // roof
  rect(0, 45, W, 8, "#262f35");

  // pillars
  for (let x = 50; x < W; x += 90) {
    rect(x, 53, 7, 107, "#303b42");
    rect(x + 5, 53, 2, 107, "#151a1f");
  }

  // warm light
  rect(173, 70, 36, 56, "#5f3827");
  rect(178, 75, 26, 46, "#b36b3c");

  // wet reflection
  ctx.globalAlpha = 0.18;
  rect(181, 166, 20, 60, "#d08a53");
  ctx.globalAlpha = 1;
}

function drawRain() {
  for (const drop of state.rain) {
    rect(drop.x, drop.y, 1, drop.length, "#8094a2");
  }
}

function render() {
  drawScene();
  drawRain();
}

let previous = performance.now();

function frame(now) {
  const dt = Math.min((now - previous) / 1000, 0.05);
  previous = now;

  update(dt);
  render();

  requestAnimationFrame(frame);
}

init();
requestAnimationFrame(frame);
```

This is only a skeleton.

The quality comes from scene design, not from the animation loop.

---

# Quality Bar

A finished scene should satisfy most of these:

## Visual

- consistent pixel grid
- clear focal point
- coherent perspective
- restrained palette
- believable architecture
- strong silhouettes
- visible foreground / midground / background separation
- coherent lighting
- atmospheric depth
- deliberate imperfections
- no obvious procedural repetition

## Animation

- stable frame rate
- movement based on delta time
- restrained motion
- no random flickering
- meaningful rare events
- convincing particles
- no distracting perpetual motion

## Audio

- user-initiated playback
- several subtle ambient layers
- no clipping
- no obvious loop seam
- sensible relative volumes
- sound synchronized with major scene events
- mute control always available

## Engineering

- renderer and update logic separated
- state is centralized
- no unnecessary dependencies
- responsive canvas scaling
- no image smoothing
- deterministic visuals where practical
- static work cached when useful
- no large per-frame allocations

---

# Final Test

Before declaring the work finished, ask:

> If I mute the page and freeze the animation, is the frame already visually compelling?

If not, improve the composition.

Then ask:

> If I restore animation but keep it muted, does the movement add life without demanding attention?

If not, reduce motion.

Then ask:

> When sound is enabled, does it make the environment feel larger and more physical rather than merely louder?

If not, improve the soundscape.

The ideal result is a scene where **visual composition carries the experience, motion creates life, and sound creates presence**.

---

# Mental Model

Think of the implementation as:

```text
pixel-art illustrator
+
cinematographer
+
2D game renderer
+
motion designer
+
ambient sound designer
```

not:

```text
frontend developer adding pixel-art decorations
```

The scene is the product.
