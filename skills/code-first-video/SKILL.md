---
name: code-first-video
description: Plan, implement and verify original videos with deterministic HTML/SVG/Canvas scenes, native PocketHarness rendering, optional existing Remotion or Blender projects, narration and sound.
---

# Code-first video direction

Make the idea visible: a diagram changes, an object moves for a reason, or a process unfolds. Text cards alone rarely explain a process. Establish the audience, message, duration, aspect ratio and delivery format before coding.

## Choose an available renderer

Run `pocket kit` for installed commands and `pocket kit probe` to inspect the host. For a compact new 2D scene, use plain HTML with SVG or Canvas and `pocket kit video`; Chromium and FFmpeg are optional host executables, not bundled dependencies. Use an existing Remotion or Blender project when its scene complexity or existing assets justify that stack. Do not invent `video_project`, `video_render`, `video_qa`, `narration_tts` or a missing script: these are not PocketHarness tools.

1. State the factual claim and evidence. For each beat, record what changes, cue seconds, short labels and the reason for the motion. A short clip needs a short plan.
2. Keep one timeline in source. Implement `window.renderFrame(seconds)` as an absolute-time function. Await local assets through `window.renderReady` when needed. Read the motion-graphics-production skill for the bundled editable HTML starter and exact renderer contract.
3. Render a low-resolution short preview, then inspect settled and transition frames. Correct clipping, hierarchy and continuity before spending time on final encoding.
4. Add supplied narration or an available local TTS tool only if needed. Measure its duration and move visual cues to the actual words. Use the procedural-audio skill for native sound synthesis; do not assume automatic narration, captions or ducking.
5. Render the final, decode it, inspect frames and review playback/audio when available. Report any sensory review that was not possible.

```bash
pocket kit frame scene.html settled.png --time 3 --size 1280x720
pocket kit video scene.html preview.mp4 --size 640x360 --fps 24 --duration 6
pocket kit video scene.html final.mp4 --size 1280x720 --fps 30 --duration 6 --audio soundtrack.wav
```

Renders over 120 s are made in pieces: `--start S --duration D` renders seconds S..S+D (audio is cut to match), then join pieces with `ffmpeg -f concat -c copy`. The time budget scales with frame count; `--timeout` raises it (max 3600 s). A single stalled frame is retried once. After the final render run `pocket kit vcheck final.mp4 --duration D`: it reports missing streams, wrong duration, black or frozen spans, silence and clipping, and exits non-zero on faults. It does not judge content.

Omit `--audio` for a silent clip. The source must expose the renderer contract before running these commands. Read [production pipeline](references/production-pipeline.md), [visual language](references/visual-language.md), [narration and sound](references/narration-and-sound.md) or [QA loop](references/qa-loop.md) only as needed. A successful render verifies encoding, not visual quality. Deliver editable sources, local assets and the requested video, with observed dimensions/duration and material limitations.
