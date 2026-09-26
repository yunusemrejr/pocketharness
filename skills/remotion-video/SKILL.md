---
name: remotion-video
description: Implement or debug an existing Remotion React/TypeScript video project with deterministic frame-based animation, local assets, measured audio timing and verified exports.
---

# Remotion video implementation

Use this when the project already uses Remotion or specifically needs its React ecosystem. Inspect `package.json`, the lockfile, registered compositions and installed command help first. PocketHarness does not bundle Remotion, a YunusPi template, `video_project` or `video_render`. For a compact new HTML/SVG/Canvas scene, consider the motion-graphics-production skill and `pocket kit video` before adding a framework.

Keep each frame a function of the composition frame and props. Use one timeline and seeded randomness. Await fonts and assets through the installed Remotion version's readiness APIs; do not rely on wall-clock timers or state accumulated by earlier frames. Keep remote assets out of deterministic exports unless their bytes are pinned and available locally.

Render through the project's existing scripts or installed Remotion CLI. Inspect a still, a short range and the final video before delivery. Check actual audio timing, first/last frames, clipping and decoder output. Do not claim that a template supplies captions, ducking, camera primitives or typography helpers unless those implementations exist in the project.

Read [implementation patterns](references/remotion-patterns.md) for compact examples. Check [official rendering documentation](https://www.remotion.dev/docs/render) for the installed version's commands. Report additional runtime requirements and verify the current license if it matters to the intended use; do not infer licensing terms from this guide.
