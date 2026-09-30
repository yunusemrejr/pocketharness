# Rendering and delivery

PocketHarness exports a local HTML scene through one Chromium process and FFmpeg. It needs those installed host executables; no Node, Playwright or npm project is required. Check `pocket kit probe` and current `pocket kit` usage before choosing this route.

```bash
pocket kit frame scene.html settled.png --time 3 --size 1280x720
pocket kit video scene.html output.mp4 --size 1280x720 --fps 30 --duration 6
```

Frame export needs only Chromium. Optional `--audio soundtrack.wav` attaches a prepared local soundtrack to a video. Keep asset files with the source. A render can only use resources available under the current tool filesystem/network permissions; bundle required assets locally for reproducibility.

Custom Canvas/WebGL or scripted motion should expose `window.renderFrame(seconds)`, which may return a Promise. Pure CSS/WAAPI and embedded-video scenes can use the native seeking fallback. If assets need asynchronous setup, assign `window.renderReady` a Promise. The renderer waits for readiness and invokes the frame function at explicit times. Each custom call must clear and reconstruct Canvas pixels or set every animated SVG/DOM property: do not accumulate frame deltas. Use seeded randomness and a fixed seed, not wall-clock time. Pause autonomous CSS, WAAPI and media playback during export.

Native export now captures CSS/WAAPI animations before asynchronous readiness and seeks embedded `<video>` elements instead of leaving them on their first frame. Footage is paused and muted; audio comes from the explicit `--audio` mix. A video's `data-render-start="2"` places its start at scene second 2, and `data-render-offset="1.5"` selects its source second 1.5. Before the cue it holds that selected source frame; after the source ends it holds its last frame, or wraps when the element has `loop`. Set visibility in `renderFrame` or the scene timeline. This also works in a scene containing only a video element. Decode/seek failures stop export and preserve a previous completed output.

Required `<img>` assets must decode successfully before export. Mark an image `data-render-optional` only when the design has an intentional fallback; this exception skips its readiness gate. Keep footage and model dependencies local. For glTF/GLB, `kit asset inspect FILE` catches missing resources and invalid ranges before browser setup, and [the model preview](../../threejs/assets/model-preview.html) demonstrates deterministic 3D asset loading and motion.

Copy [the six-second starter](../assets/timeline.html) into the project and change its visual content. `renderFrame()` sets an export class that hides preview controls. `?preview=1` enables its interactive replay button. The starter is a rendering contract, not a finished visual design. It uses local system fonts; bundle a licensed font when identical typography across hosts matters.

Render a small preview first. Inspect first/last frames, settled beats, and times on both sides of transitions. Check metadata and decode with `ffprobe`/`ffmpeg`, then review continuous playback. For looped output, the final sampled frame is before the endpoint; do not append a duplicate endpoint frame to hide a timing error.

Prefer an existing Blender/Remotion pipeline for modeling, rigging, alpha/HDR or a project already using that renderer. Local WebGL models can use the deterministic browser export when that meets the output requirements. Audio-reactive motion should use a measured, smoothed envelope indexed by absolute time; a beat grid is an estimate until checked against the track.

Primary references: [FFmpeg](https://ffmpeg.org/ffmpeg.html), [ffprobe](https://ffmpeg.org/ffprobe.html).
