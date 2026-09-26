---
name: physical-animation-systems
description: Build physically coherent animated springs, ropes, cloth, contacts and coupled motion with stable stepping, units and numerical diagnostics. Use when physical behavior matters, not for ordinary UI easing.
---

Choose the intended fidelity: art-directed plausibility, interactive simulation, or quantitatively faithful mechanics. Keep deliberate exaggeration identifiable; do not impose conservation or realistic gravity on decorative UI motion.

- For timestep ownership, spring tuning, stable integration and multi-rate scenes, read [stepping and springs](references/stepping-and-springs.md).
- For constraints, impacts, friction and diagnosing apparent physical errors, read [constraints and diagnostics](references/constraints-and-diagnostics.md).
- Use `node <this-skill>/scripts/physics-check.mjs --self-test` to check the helper, or import its spring tuning, exact critical spring and XPBD correction functions. It is a numerical reference, not a collision engine.

Record units, coordinate handedness, up axis, simulation rate and presentation rate before coupling systems. Evaluate the same physical duration at several render rates; compare solver rates separately. A smooth screenshot cannot establish stability. Choose observable tolerances such as maximum stretch, penetration, settling time or energy residual, and demonstrate only the checks relevant to the requested effect.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
