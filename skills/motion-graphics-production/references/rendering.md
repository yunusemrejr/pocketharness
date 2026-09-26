# Rendering and delivery

PocketHarness exports a local HTML scene through one Chromium process and FFmpeg. It needs those installed host executables; no Node, Playwright or npm project is required. Check `pocket kit probe` and current `pocket kit` usage before choosing this route.

```bash
pocket kit frame scene.html settled.png --time 3 --size 1280x720
pocket kit video scene.html output.mp4 --size 1280x720 --fps 30 --duration 6
```

Frame export needs only Chromium. Optional `--audio soundtrack.wav` attaches a prepared local soundtrack to a video. Keep asset files with the source. A render can only use resources available under the current tool filesystem/network permissions; bundle required assets locally for reproducibility.

The page must expose `window.renderFrame(seconds)`, which may return a Promise. If assets need asynchronous setup, assign `window.renderReady` a Promise. The renderer waits for readiness and invokes the frame function at explicit times. Each call must clear and reconstruct Canvas pixels or set every animated SVG/DOM property: do not accumulate frame deltas. Use seeded randomness and a fixed seed, not wall-clock time. Pause autonomous CSS, WAAPI and media playback during export.

Copy [the six-second starter](../assets/timeline.html) into the project and change its visual content. `renderFrame()` sets an export class that hides preview controls. `?preview=1` enables its interactive replay button. The starter is a rendering contract, not a finished visual design. It uses local system fonts; bundle a licensed font when identical typography across hosts matters.

Render a small preview first. Inspect first/last frames, settled beats, and times on both sides of transitions. Check metadata and decode with `ffprobe`/`ffmpeg`, then review continuous playback. For looped output, the final sampled frame is before the endpoint; do not append a duplicate endpoint frame to hide a timing error.

Use an existing Blender/Remotion pipeline for genuine 3D, alpha/HDR or a project already using that renderer. Do not force those requirements into an ordinary H.264 export. Audio-reactive motion should use a measured, smoothed envelope indexed by absolute time; a beat grid is an estimate until checked against the track.

Primary references: [FFmpeg](https://ffmpeg.org/ffmpeg.html), [ffprobe](https://ffmpeg.org/ffprobe.html).
