# Animated rendering and ownership

## Linear light and color animation

Keep lighting computations in the renderer's working linear color space. Annotate color textures as sRGB where appropriate; normal/roughness/metalness data are not sRGB color. `Color` construction and conversion behavior depends on the color-management configuration; avoid a second manual sRGB conversion after the API has already converted. Interpolate linear RGB for a light-energy transition; choose a perceptual color space deliberately for a design transition. These are different goals. Apply display conversion once at the end of a postprocessing chain. Tone mapping maps scene luminance to display range and is a chosen look, not a requirement for every PBR scene. [Color API](https://threejs.org/docs/pages/Color.html), [renderer API](https://threejs.org/docs/pages/WebGLRenderer.html).

**Worked color example.** Equal linear-light mixing of black and white gives linear 0.5, encoded sRGB approximately 0.7354 using `1.055*0.5^(1/2.4)-0.055`. Averaging their encoded sRGB values gives 0.5, equivalent to only about 0.2140 linear light. Establish which midpoint the art direction wants before diagnosing a dark fade as a shader bug.

## Instancing and GPU animation

Repeated geometry/material can use `InstancedMesh`. Set `instanceMatrix.needsUpdate` once after a batch of `setMatrixAt` changes, and similarly mark instance color changes. Update or conservatively bound the animated instance extents; stale bounds cause disappearance and missed raycasts. Several materials, shadows and multiple passes can still create several draws. Instancing reduces submission overhead, not per-vertex work or fragment overdraw. [InstancedMesh API](https://threejs.org/docs/pages/InstancedMesh.html).

For periodic swaying or orbital motion, upload immutable phase/frequency/amplitude attributes and a shared time uniform; evaluate positions in the shader instead of uploading matrices each frame. Give the depth/shadow material the same deformation, provide conservative bounds, and define CPU picking behavior. GPU visual deformation does not automatically update physics or CPU raycasts. Large elapsed times can lose precision: use a bounded phase or split time representation without introducing a discontinuity in the intended trajectory. Skinned crowds require an explicit shared-rig/animation-texture strategy, not assuming generic mesh instancing duplicates independent skeleton state.

**Worked bandwidth example.** 10,000 4×4 float32 matrices occupy 640,000 bytes; uploading all at 60 Hz is 38.4 MB/s payload, before driver overhead. Four static float32 parameters per instance occupy 160,000 bytes once, plus a time uniform each frame. This is an opportunity, not a promised speedup: shader arithmetic, shadow passes and fill rate can dominate. A 1920×1080 canvas at DPR 2 has 8,294,400 pixels per pass, four times DPR 1. Measure resolution changes with identical scene conditions.

## Ownership and lifecycle

Stop the owning animation loop, unsubscribe events/observers and reject late async asset attachment using a generation/disposed flag. Stop actions before uncaching their root. Disposing a mesh material does not dispose shared texture ownership; maintain an explicit asset owner or reference counts. Dispose geometry, textures, render targets, skeleton resources and controls only when their final consumer releases them. Avoid traversing arbitrary material property values and calling every `dispose` method; that can destroy shared assets. [Material API](https://threejs.org/docs/pages/Material.html), [Texture API](https://threejs.org/docs/pages/Texture.html), [mixer API](https://threejs.org/docs/pages/AnimationMixer.html).

`renderer.dispose()` belongs to the renderer owner. Do not force a shared WebGL context loss on routine child removal. Track resource counts after repeated mount/unmount cycles and after asynchronous loads finish, comparing to a warmed baseline; internal renderer resources mean zero is not a universal target. A texture may additionally own an ImageBitmap needing `close()` after its final consumer; confirm ownership before closing it.

## Performance evidence

Separate CPU animation, scene traversal, GPU render duration and presentation latency. Compare p50/p95 after warmup under representative animation and viewport sizes. GPU timer availability and disjoint samples matter; CPU time around `render()` alone does not measure completed GPU work. Preserve a pause/reduced-motion option appropriate to the scene without silently changing scientific playback data.

Sources checked 2026-09-09. Numeric costs are payload estimates, not benchmark results.
