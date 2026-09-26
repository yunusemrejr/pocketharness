---
name: procedural-animation-math
description: Calculate smooth procedural motion, curve timing, quaternion orientation and seamless loops for animated paths, cameras and objects. Use for continuity, speed or orientation problems beyond ordinary easing presets.
---

Define the animated quantity, coordinate frame, units, duration and desired endpoint behavior before choosing a curve. Distinguish continuity of geometric shape from continuity of motion in seconds. Use the smallest model that captures the desired effect.

- Read [curves and timing](references/curves-and-timing.md) for Bézier/Hermite derivatives, joins, arc length and acceleration budgets.
- Read [orientation and loops](references/orientation-and-loops.md) for shortest-arc quaternions, frame transport and seam conditions.
- Import `scripts/motion-math.mjs` for validated cubic evaluation/derivatives, shortest-arc slerp and quintic easing. Run it with `--self-test` for its numerical invariants. It does not parse SVG paths or replace a production spline library.

Prefer direct evaluation from absolute time for seekable choreography. Integrate only genuinely stateful motion. Evaluate endpoints and derivatives numerically as well as visually; a loop can match positions while snapping velocity. Preserve intentional holds, impacts and art-directed discontinuities rather than smoothing them away automatically.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
