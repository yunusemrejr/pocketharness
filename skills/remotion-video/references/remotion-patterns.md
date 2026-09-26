# Remotion patterns

## Timing from the master timeline

```tsx
const frame = useCurrentFrame();                 // scene-relative inside a Sequence
const { fps, durationInFrames } = useVideoConfig();
const build = progress(frame, cue(scene, "network"), 45);   // 0..1 over 45 frames from the cue
const items = labels.map((_, i) => stagger(frame, cue(scene, "list"), i, 5, 18));
const out = exitFade(frame, durationInFrames);    // clear before the cut
```

Staggered primitives (`TokenRow`, `Matrix`, `BarChart`, `Graph`) take linear group progress from `sweep()` and ease each item with `groupItem()`, so the last item completes exactly at the end of the sweep. Passing an eased `progress()` instead crowds the whole stagger into the first moments.

`cue()` throws on a missing cue, so a renamed cue fails the render instead of silently desynchronizing. Add cues to `video.json` and re-time them after narration is synthesized.

## Animation recipes

- Draw-on lines and paths: `strokeDasharray={len} strokeDashoffset={len * (1 - t)}`.
- Counting numbers: `<Counter to={target} progress={p} />` (tabular monospace figures, no jitter), or `Math.round(interpolate(t, [0, 1], [0, target]))` inline.
- Lower thirds: `<LowerThird name="…" title="…" progress={hold(p)} />` for speaker captions; keep name + title under ~6 words.
- Stepped explanations: `<ProgressBar progress={p} steps={["One", "Two", "Three"]} />` derives the active step, so narration and motion agree.
- Diagram annotation: `<Callout from={[0.3, 0.4]} to={[0.6, 0.2]} label="…" progress={p} />` draws its elbow line, then fades the label in.
- Morph between layouts: interpolate positions of the same element ids from layout A to layout B with `ease.inOut`.
- Camera moves: wrap the scene content in a `div` with `transform: translate(x, y) scale(s)` driven by eased progress. Move the world, not every element.
- Emphasis: `settle(frame, fps, at)` for a spring overshoot ≤ 8%. Avoid continuous wobble.
- Masked text reveal: `overflow: hidden` wrapper plus a `translateY` of the inner line (see `Heading`).
- Scene continuity: pass the same data and positions into consecutive scenes, and let the outgoing scene skip its exit fade when the next one continues the object.
- Scene entries: `transition.type` in `video.json` supports `fade`, `slide` (horizontal), `slideup`/`slidedown` (vertical), `wipe`, `zoom`, `blur` and `none`. Match the direction to the reading flow.

## Beat-synced and looping motion

`src/timing.ts` (re-exported by `motion.ts`) holds beat and loop helpers with no imports. Use the same `bpm` as the `audio_synth` music bed so picture and track share a clock:

```tsx
const thump = pulse(frame, fps, 84);            // 1 on the beat, decaying after
const n = beatCount(frame, fps, 84);            // whole beats elapsed: step items per beat
style={{ transform: `scale(${1 + 0.04 * thump})` }}
```

`beat()` gives the 0..1 phase inside the current beat for anything that should cycle per beat. `loopProgress(frame, frames)` is a seamless 0..1 saw for loops (rotation, marquee, shimmer); `pingpong(frame, frames)` is a 0..1..0 triangle for oscillation (hover, breathing). `hold(p, attack, release)` wraps a 0..1 progress with read/hold/clear fades for lower thirds and callouts. One synced accent per beat at most; if everything pulses, nothing does.

## SVG vs Canvas vs Three.js

| Use | For |
| --- | --- |
| SVG | Diagrams, graphs, typography-heavy illustrations, anything with crisp strokes and labels (hundreds of nodes) |
| Canvas 2D | Particles, dense grids, noise fields, trails (thousands of elements). Draw in `useLayoutEffect` keyed on the frame; seed randomness |
| Three.js | Genuine 3D (spatial relationships, depth, orbiting structure). Add `@remotion/three` + `three` pinned to the project's Remotion version, render inside `<ThreeCanvas>`, and derive camera and object transforms from the frame. Use the existing `scene_render` tool instead for quick standalone 3D shots |

Keep line widths ≥ 2 px and labels ≥ 26 px at 1080p. Previews are rendered at half scale, so hairlines vanish.

## Audio

`Main.tsx` plays `scene.narrationAudio` inside each scene's `Sequence` offset by `narrationOffset`, the music with a ducking volume function, and `audio.sfx` at absolute seconds. Volume callbacks receive the frame. Keep them pure. To add a per-scene sound, prefer `audio.sfx` in `video.json` over hard-coded `<Audio>`, so timing stays in one place.

## Rendering

- `video_render mode:"stills"`: frames at 60% of each scene, or `scene` + `count`, or explicit `times`. Returns a contact sheet. The bundle is cached by content hash, so re-renders after small edits are fast.
- `video_render mode:"preview" scene:"<id>"` (or `from`/`to` seconds): half-scale, CRF 28.
- `video_render mode:"final"`: full scale, CRF 18, H.264 + AAC 192 kbps, decode-verified. Long videos take minutes; progress streams while rendering.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| `unknown component` | Register it in `src/scenes/index.ts` with the exact name used in `video.json` |
| `has no cue` | Add the cue to that scene's `cues` in `video.json` |
| Fonts fall back to serif or sans | Import the weight in `src/fonts.ts`; use the family name exactly as `theme.display`/`text`/`mono` |
| Canvas blank in stills | Draw in `useLayoutEffect`, not `useEffect`; include `frame` in the dependency list |
| Flicker between frames | Unseeded randomness or state carried between frames; derive everything from the frame |
| Render timeout | Reduce per-frame work (canvas instead of SVG for dense fields), or render a range |
| Audio missing in final | `narrationAudio` path must be relative to `public/`; run `video_project check` |
| Dependencies missing | `video_project action:"install"` |

Remotion is free for individuals and small teams; larger companies need a company license (see remotion.dev/license). Mention this when delivering a project to an organization.
