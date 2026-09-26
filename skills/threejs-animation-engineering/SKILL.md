---
name: threejs-animation-engineering
description: Engineer Three.js character, camera and procedural 3D animation with clip blending, root motion, quaternion transforms, projection calculations, instancing and owned-resource cleanup. Use for animated Three.js scenes and animation defects, not ordinary static CSS.
---

# Three.js animation engineering

Start from the installed Three.js revision and renderer backend. Preserve the scene's coordinate convention, world scale and intended motion. Animation, physics and rendering need an explicit update order and a shared time policy.

- For clips, root motion, bones, camera trajectories and calculated screen size, read [motion and projection](references/motion-and-projection.md).
- For animated crowds, GPU data, color transitions and teardown, read [rendering and ownership](references/rendering-and-ownership.md).

Keep one owner per animated property. If a mixer, physics engine and input controller all write the character position, separate a world/controller parent from the animated model child. Solve the disagreement instead of changing update order until it appears stable.

Verify the requested result at a loop seam, transition midpoint, interrupted transition and changed viewport; include pause/resume when the scene supports it. For physical motion, compare numeric invariants as well as screenshots. Do not prescribe a new renderer or dependency unless it solves the measured problem.
