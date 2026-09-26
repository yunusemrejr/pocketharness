---
name: remotion-video
description: Implement programmatic video in Remotion (React/TypeScript) inside a YunusPi video project; deterministic frame-pure animation, scene components driven by a master video.json timeline, reusable SVG/Canvas primitives, optional Three.js, audio layering, and rendering stills, previews and finals through video_render. Use when writing or debugging Remotion compositions, scenes, primitives or render failures.
---

# Remotion video implementation

Start from `video_project action:"init"`. The template's contract:

- `video.json` owns timing and content. `src/timeline.ts` derives frames from it (`timeline()`, `cue(scene, name)`, `toFrames()`); scene components never hard-code seconds that narration depends on.
- `src/scenes/index.ts` registers component names used by `video.json`. Each scene receives `scene` (with `durationInFrames`, cues) plus its `props`.
- `src/primitives/` holds reusable visuals: `Stage`, `Backdrop`, `ParticleField`, `Heading`, `KineticText`, `LowerThird`, `Counter`, `ProgressBar`, `Callout`, `TokenRow`, `NeuralNet`, `Matrix`, `Graph`, `BarChart`, `TimelineAxis` and `CodeBlock`. They take data and 0..1 progress values and contain no timing.
- Finishing and sound-driven layers: `Captions` (narration captions, `chunks` or `karaoke`), `AudioSpectrum` (bars driven by a public/ audio file through `@remotion/media-utils`), `FilmGrain`, `LightLeak`, `CameraMove` (push-in, pull-out, pans, drift over `scene.durationInFrames`) and `Glitch` (a brief channel-split at a cue). One texture per scene; motion must explain, not decorate.
- `video.json` `captions: {enabled, style, maxWords, position}` burns captions in from the narration text and measured `narrationSeconds` (timing comes from `src/captions.ts`: syllable-weighted words with punctuation pauses, not speech alignment); `video_render mode:"final"` also writes `captions.srt` and `captions.vtt` with the same timing. Per-scene `transition: {type: fade|slide|slideup|slidedown|wipe|zoom|blur|none, seconds}` animates the scene's entry.
- `src/motion.ts` (`progress`, `sweep`, `groupItem`, `stagger`, `enter`, `settle`, `exitFade`) and `src/timing.ts` (`beat`, `pulse`, `beatCount`, `loopProgress`, `pingpong`, `hold`) plus `src/theme.tsx` (`useTheme`, `type`, `space`, `SAFE`, seeded `rng`, color `mix`) define the shared vocabulary. Drive beat-synced motion from `pulse(frame, fps, bpm)` with the same bpm as the `audio_synth` bed.
- `Main.tsx` sequences scenes and layers narration, music (auto-ducked under narration) and sfx. `scene-<id>` compositions exist for isolated previews.

Rules that keep renders correct:

1. Every visual value is a pure function of `useCurrentFrame()`. No `Date.now()`, no `Math.random()` (use `rng(seed)`), and no state that accumulates between frames.
2. Canvas drawing happens in `useLayoutEffect` keyed on the frame, clearing first. Size canvases from `useVideoConfig()`.
3. Fonts come from the pinned `@fontsource` imports in `src/fonts.ts`, which delay rendering until loaded. Never rely on system or network fonts.
4. Assets live in `public/` and load with `staticFile()`. No remote URLs.
5. Parameterise: a scene takes data through props and computes progress from cues. Duplicate-then-tweak scenes become inconsistent; extract a primitive instead.
6. Keep React trees light. Thousands of SVG nodes render slowly; switch dense fields (particles, grids > ~2,000 cells) to canvas.

Read [Remotion patterns](references/remotion-patterns.md) for animation recipes, SVG/Canvas/Three.js integration, audio, render settings and troubleshooting.

Rendering goes through `video_render` (stills and contact sheet, scene or range previews, final). Do not install Remotion globally or run the Remotion CLI by hand unless diagnosing the tool. The code-first-video skill owns the review loop: after every meaningful change, render stills and actually look at them.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
