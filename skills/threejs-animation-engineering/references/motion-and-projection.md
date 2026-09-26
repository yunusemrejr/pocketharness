# Motion, transforms and projection

## Mixer and action state

`mixer.update(dt)` takes seconds. Convert an animation-loop millisecond timestamp once; initialize the first delta to zero. Define whether a hidden tab pauses presentation time or catches up from an authoritative simulation clock. Clamping wall time is a presentation policy, not a valid replacement for elapsed time in scientific playback.

Use `mixer.clipAction(clip, root)` to reuse actions. At an actual transition, prepare the destination once: `next.reset().setEffectiveTimeScale(1).setEffectiveWeight(1).play(); next.crossFadeFrom(previous, duration, false);`. Do not reset or schedule fades every frame. An interrupted transition may have several partial weights: retain the current pose/weights, fade the participating actions, or use an explicit blend controller. Blindly restarting two clips can pop. Crossfade warping changes playback speed; it does not solve foot contact or locomotion matching. For a one-shot, use `setLoop(THREE.LoopOnce, 1)` and `clampWhenFinished = true`; clamp only applies when the action actually finishes. [Action API](https://threejs.org/docs/pages/AnimationAction.html), [mixer API](https://threejs.org/docs/pages/AnimationMixer.html).

**Worked blend example.** A 0.4 s linear transition sampled 0.1 s after its start has blend factor 0.25: outgoing/incoming weights 0.75/0.25, provided both started with the intended effective weight and no other actions contribute. A 1.2 s walk clip and 0.8 s run clip do not have the same gait phase at equal local time. At walk time 0.3 s, phase is 0.25; corresponding run time is 0.2 s. Phase matching still needs contact annotations if their heel-strike timings differ.

## Root motion and skinned rigs

Extract root displacement in the authored local frame and apply it once to the controller's world transform. Remove that displacement from the visual child or use an in-place clip. For a forward loop crossing duration `T`, use `(p(T)-p(previous)) + (p(current)-p(0))`; add full-cycle displacement for additional wraps. Raw `p(current)-p(previous)` teleports backward at the seam. Reverse playback and ping-pong need their own signed segment accounting. For turning motion, accumulate rigid transform deltas rather than adding translations from changing coordinate frames.

Use `q_world = q_parent * q_local`; consequently `q_local = inverse(q_parent) * q_world` for a pure rotation hierarchy. Quaternion multiplication is not commutative. Use normalized quaternions and spherical interpolation; shortest-path sign correction uses `dot(q0,q1)<0` to negate one representation. `q` and `-q` describe the same orientation. Parent nonuniform scale/shear requires matrix-aware handling; simply extracting a quaternion cannot preserve shear. [Quaternion API](https://threejs.org/docs/pages/Quaternion.html).

Bone animation usually operates in parent-local bind space. Apply procedural offsets consistently before/after the authored pose, with explicit choice of local or world axes. Do not rotate the same bone once through a mixer track and again by accumulating yesterday's offset. Restore the base pose each update. For shared skeletons/clones verify skeleton ownership and unique bone transforms; object clones alone may share rig resources.

**Worked root example.** A clip travels 1.2 m over 0.8 s, so its average authored speed is 1.5 m/s. Requested 2.25 m/s gives time scale 1.5 if stride length remains fixed. At a seam, `p(previous)=1.17 m`, `p(current)=0.03 m`, start=0 and end=1.2: correct delta is 0.06 m, not −1.14 m. This speed adjustment alone does not guarantee no sliding when acceleration or terrain changes; test contact velocity in world space.

## Camera and pixel calculations

For an unshifted perspective camera with effective vertical FOV `theta`, viewport height `H`, and point at positive camera-axis depth `z`, visible height is `2*z*tan(theta/2)`. A small vertical object of height `h` at constant depth spans approximately `P = H*h/(2*z*tan(theta/2))` pixels. Use CSS height for CSS pixels or drawing-buffer height for device pixels; Euclidean distance to an off-axis point is not `z`. For finite objects with varying depths, project their corners. Use effective FOV when zoom differs from one, and update the projection matrix after camera parameter changes. [PerspectiveCamera API](https://threejs.org/docs/pages/PerspectiveCamera.html).

**Worked framing example.** At FOV 60°, H=1080 CSS px, a 2 m tall plane at z=5 m spans `1080*2/(10*tan(30°)) = 374.12 px`. To make it 600 px tall, z=3.1177 m. At device pixel ratio 2 the drawing-buffer height doubles, but CSS size remains 600 px. With aspect 16:9, horizontal FOV is `2*atan(tan(30°)*16/9)=91.49°`, not 106.67°.

For a conventional perspective fixed-point depth buffer with `b` bits, local resolution is approximately `deltaZ = z²*(f-n)/(f*n*(2^b-1))`. This estimate is not for reversed floating-point depth. With n=0.1 m, f=1000 m, b=24, z=100 m, deltaZ≈0.00596 m; moving n to 1 m gives ≈0.000595 m. Push the near plane outward as far as composition permits before enabling a different depth strategy. No universal near/far ratio guarantees absence of z-fighting; coplanar surfaces still collide. Test the selected backend, depth format and extension availability.

## Acceptance evidence

Check frame-rate invariance over an identical elapsed interval, loop-seam displacement, blend continuity and quaternion norm. Test root motion with a rotated parent and camera framing after resize. Use tolerances derived from world scale or screen error, rather than exact floating-point equality. A render screenshot cannot establish correct root-motion accumulation.

Sources checked 2026-09-09. Formulas and numeric examples are derived here; API availability must match the installed revision.
