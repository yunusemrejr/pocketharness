---
name: procedural-animation-math
description: Calculate smooth procedural motion, curve timing, quaternion orientation and seamless loops for animated paths, cameras and objects. Use for continuity, speed or orientation problems beyond ordinary easing presets.
---

Define the animated quantity, coordinate frame, units, duration and desired endpoint behavior before choosing a curve. Distinguish continuity of geometric shape from continuity of motion in seconds. Use the smallest model that captures the desired effect.

- Read [curves and timing](references/curves-and-timing.md) for Bézier/Hermite derivatives, joins, arc length and acceleration budgets.
- Read [orientation and loops](references/orientation-and-loops.md) for shortest-arc quaternions, frame transport and seam conditions.
- Implement the needed formula from those references in the project's own language. No `scripts/motion-math.mjs` is bundled. For a quintic ease on a clamped `t` in `[0,1]`, use `t*t*t*(t*(6*t-15)+10)`; check values 0/1 and zero first/second endpoint derivatives. For quaternion interpolation test identical, antipodal and nearly identical inputs with unit-length output.

Prefer direct evaluation from absolute time for seekable choreography. Integrate only genuinely stateful motion. Evaluate endpoints and derivatives numerically as well as visually; a loop can match positions while snapping velocity. Preserve intentional holds, impacts and art-directed discontinuities rather than smoothing them away automatically.

For exportable browser scenes, preserve the `window.renderFrame(seconds)` contract from the motion-graphics-production skill. Check the same timestamp after forward and backward seeks; a repeated frame should not depend on the path taken to reach it.
