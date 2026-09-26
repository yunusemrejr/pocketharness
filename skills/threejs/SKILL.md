---
name: threejs
description: Three.js / WebGL 3D for the web — scene setup, lighting and color, voxel/low-poly composition, draw calls, resource ownership, glTF assets, shaders and failure diagnosis. Use when adding 3D scenes or models, including voxel art, or fixing WebGL artifacts, performance or leaks.
---

# Three.js

## Scene setup

Check the installed revision and renderer backend before copying examples. Use a single owned animation loop; convert elapsed milliseconds to seconds for motion. Resize the drawing buffer and camera aspect together and call `camera.updateProjectionMatrix()`. Choose pixel ratio from a measured quality/performance budget: DPR 2 costs four times the pixels of DPR 1, but 2 is not a correctness limit. Set a consistent world scale and keep the perspective near plane positive. Push near outward and far inward where composition permits; no fixed ratio guarantees freedom from z-fighting. [Renderer](https://threejs.org/docs/pages/WebGLRenderer.html), [camera](https://threejs.org/docs/pages/PerspectiveCamera.html).

For clip blending, root motion, camera calculations, animated instancing and teardown, use [threejs-animation-engineering](../threejs-animation-engineering/SKILL.md).

## Voxel and low-poly art

Use this workflow for a Three.js/WebGL voxel or deliberately low-poly scene. Block the scene on a coarse grid before adding detail: choose a readable 3/4 silhouette, focal mass, negative space, ground contact and camera framing. Inspect the target viewport and a thumbnail so the composition survives scale changes.

- **Palette and materials:** use a small role-based palette (base, shadow, highlight, accent) with value separation before texture detail. Prefer flat or nearest sampling, or vertex colors, for pixel-art surfaces. Choose Lambert for simple lit forms or Standard when roughness and metalness matter. Share materials; do not allocate one material per voxel.
- **Surface choice:** keep boxes for small or irregular sets; for larger static volumes cull hidden faces and merge compatible faces or geometry. Use `InstancedMesh` for repeated decorations. Keep animated or independently edited pieces separate, preserve hard or flat normals, and avoid coplanar overlays.
- **Light and verify:** one broad key with hemisphere or ambient fill and soft contact shadows usually keeps small faces readable. Set background contrast and restrained exposure. Render actual pixels through existing project, browser or render tools; check silhouette and occlusion, palette values, seams or z-fighting, clipping, shadow direction, nearest filtering and transparency. Fix the largest readability issue first. If only source or DOM evidence is available, report GPU rendering as unverified. For animation or Blender authoring, read the matching task-specific skill. Do not add a voxel framework or dependency solely for the look.

## Lighting and color

Standard/Physical materials respond to suitable lights; environment maps are useful for image-based lighting and reflections, especially metals, but are not universally required to avoid black output. Start with deliberate lighting and inspect exposure, normals and material settings. Tone mapping is a scene/display choice, not a requirement for PBR materials. Keep lighting in linear space and output conversion at the end; avoid duplicate conversion in a postprocessing chain. [Standard material](https://threejs.org/docs/pages/MeshStandardMaterial.html), [Color](https://threejs.org/docs/pages/Color.html).

Color maps commonly use `SRGBColorSpace`; normal, roughness and metalness textures carry data and should not undergo sRGB decoding. Black/washed colors can arise from missing illumination, texture annotations, exposure or conversion errors; tone mapping being disabled alone does not establish a defect. [Texture](https://threejs.org/docs/pages/Texture.html).

## Assets

Prefer the project's existing glTF pipeline when appropriate. Draco/Meshopt and KTX2 can reduce transfer or GPU texture costs, with asset-dependent results; no fixed compression ratio or polygon cap fits every scene. Use loaders and decoder versions compatible with the installed Three.js, and call `KTX2Loader.detectSupport(renderer)` when using that loader. [KTX2Loader](https://threejs.org/docs/pages/KTX2Loader.html).

Power-of-two texture requirements depend on the graphics API. WebGL1 restricts NPOT wrapping and mipmapping; do not impose those rules on WebGL2 indiscriminately. Measure decoded texture memory as well as download size. [WebGL texture rules](https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/Tutorial/Using_textures_in_WebGL).

## Performance and ownership

Profile CPU work, draw submission, vertex/fragment work and resource counts before choosing an optimization. Merge compatible static geometry or use instancing where appropriate; several materials and passes can still require several draws. Reuse geometry/material/texture resources with explicit ownership. Mark modified instance attributes dirty once per batch and refresh animated bounds. [InstancedMesh](https://threejs.org/docs/pages/InstancedMesh.html).

Removing an object does not release its GPU resources. Release geometries, materials, textures and render targets only after their final consumer is gone; do not traverse arbitrary object values and dispose shared assets blindly. Dispose the renderer only at its owning lifecycle boundary. Routine child unmount does not require forced context loss. Stop loops/listeners and prevent late async loads from attaching after teardown. [Material disposal](https://threejs.org/docs/pages/Material.html), [renderer lifecycle](https://threejs.org/docs/pages/WebGLRenderer.html).

## Shader and defect checks

Use ShaderMaterial/chunks compatible with the renderer revision or the project's existing node/TSL path. Switching to WebGPU is an architectural choice, not a required upgrade for animation. Keep custom color output and vertex deformation consistent across visible, shadow and picking paths.

For black output inspect camera/frustum, lights, normals, loading failures and material. For flicker inspect coplanarity, depth precision and overlapping geometry before applying a scale-independent magic offset. For blur compare CSS and drawing-buffer dimensions. Test the relevant viewport and repeat mount/unmount for lifecycle changes.

API sources checked 2026-09-09. Verify revision-sensitive options against the installed package.
