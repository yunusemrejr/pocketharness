---
name: motion
description: Build and verify HTML/CSS/JavaScript motion graphics, timed visual sequences and interactive UI animations. Use for animation implementation, choreography, timeline debugging or reduced-motion behavior.
---

# Motion with an inspectable timeline

Follow the user's visual direction, duration and existing stack. Distinguish a narrative motion graphic from UI feedback: a short interaction should not delay work, but a story may need a long deliberate hold. Timing examples are starting points, not universal quality rules.

## Design before implementation

Define the message, focal point and sequence of beats. For each beat identify what enters, moves, holds and exits. Establish canvas/aspect ratio, duration and whether it loops. Use hierarchy and restrained simultaneous movement so text remains readable. Reuse the product's type, color and geometry; do not default to decorative gradients or constant animation.

## One timeline owner

Use the existing framework or a small CSS/JS implementation. Avoid adding an animation library merely for a few transitions. Prefer transforms and opacity when they express the effect; measure necessary layout/paint animation rather than banning it or claiming it is free.

For narrative graphics, make the visual state a function of elapsed time. Derive every track from the same clock; do not accumulate frame deltas or schedule a cascade of unrelated timers. Provide pause/replay and a way to seek exact times when needed for review/export. Seed randomness for reproducible frames. Avoid two loops or CSS and JS both owning the same animated property.

```js
// Pure track timing: independently seekable, no dependency on frame history.
const clamp01 = x => Math.max(0, Math.min(1, x));
const progress = (timeMs, startMs, durationMs) =>
  durationMs <= 0 ? Number(timeMs >= startMs)
    : clamp01((timeMs - startMs) / durationMs);
const easeOut = t => 1 - (1 - t) ** 3;
function renderAt(timeMs, title) {
  const t = easeOut(progress(timeMs, 200, 600));
  title.style.opacity = String(t);
  title.style.transform = `translateY(${24 * (1 - t)}px)`;
}
```

An interactive transition instead needs retargeting from current state: test reversal, repeated clicks and cancellation. Cancel obsolete timers/animations and clean up on unmount. Motion must not block keyboard access or leave invisible focusable controls.

## Verify actual behavior

Inspect start, key transitions, middle, end and loop seam; also play the sequence because still frames cannot prove smoothness. Check relevant viewport sizes, missing assets, text clipping and readable hold time. Wait for required fonts/assets before export. Match frame time to frame index for deterministic export, rather than relying on wall-clock screenshot delays.

Use existing renderer/browser tools. `render_see {animationTimeMs:500}` pauses current main-frame CSS/WAAPI document-timeline animations at each animation's local time; use `reducedMotion:"reduce"` to inspect that alternative. It excludes JS/rAF, scroll timelines and removed/not-yet-created animations. Samples are not playback verification. A vision-capable child can inspect supplied frames, but cannot attest to unobserved motion. Without vision, measure geometry, track values, duration and endpoint invariants in code and state that appearance remains unverified.

Provide a reduced-motion state that preserves the message and useful controls. For narrative content use a static summary or explicit playback when appropriate; do not globally shorten every animation if completion callbacks or the storyline depend on it. Check performance on the intended environment; do not infer frame rate from property names or one screenshot.
