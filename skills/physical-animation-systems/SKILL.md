---
name: physical-animation-systems
description: Build physically coherent animated springs, ropes, cloth, contacts and coupled motion with stable stepping, units and numerical diagnostics. Use when physical behavior matters, not for ordinary UI easing.
---

Choose the intended fidelity: art-directed plausibility, interactive simulation, or quantitatively faithful mechanics. Keep deliberate exaggeration identifiable; do not impose conservation or realistic gravity on decorative UI motion.

- For timestep ownership, spring tuning, stable integration and multi-rate scenes, read [stepping and springs](references/stepping-and-springs.md).
- For constraints, impacts, friction and diagnosing apparent physical errors, read [constraints and diagnostics](references/constraints-and-diagnostics.md).
- Use `pocket kit spring` for the native damped-spring helper; run `pocket kit` for its current arguments. For coupled constraints, implement the relevant formula from the references in the project's existing language and compare timestep refinement and known solutions. No `scripts/physics-check.mjs` is bundled.

Record units, coordinate handedness, up axis, simulation rate and presentation rate before coupling systems. Evaluate the same physical duration at several render rates; compare solver rates separately. A smooth screenshot cannot establish stability. Choose observable tolerances such as maximum stretch, penetration, settling time or energy residual, and demonstrate only the checks relevant to the requested effect.

The guides describe numerical methods; they do not supply a collision engine. State the tested conditions and tolerances before claiming stability.
