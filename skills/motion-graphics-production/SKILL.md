---
name: motion-graphics-production
description: Produce timed motion graphics for video, including title sequences, kinetic typography, lower thirds, explainers and audio-reactive visuals, with deterministic frame rendering and editable sources. Use motion for ordinary interactive UI animation.
---

# Motion graphics production

Establish message, aspect ratio, duration, frame rate and delivery format. Convert the story into beats with readable holds and deliberate entrances/exits. Use the existing typography and visual direction. For music or narration, map cue seconds before animating; effects should support the focal point.

Make each frame a function of absolute time. Seed randomness, wait for fonts/assets and avoid accumulated frame deltas. For browser graphics, the [timeline starter](assets/timeline.html) exposes `window.renderFrame(seconds)` and an editable beat sequence. Copy it into the project, adapt the design and retain the render function. It is a mechanical starting point, not a finished visual style.

Read [rendering and delivery](references/rendering.md) to use `pocket kit video` with installed Chromium and FFmpeg, or an existing renderer such as Blender. No Node or Playwright exporter is bundled or required by the native route. Render a small preview first; inspect actual frames for type clipping, composition and transition continuity, then encode and review playback. Audio-reactive amplitude should be smoothed and normalized from measured signals rather than raw sample peaks.

Deliver editable sources and the rendered video. Verify duration, frame count, first/last frames, loop seam and audio synchronization. A clean encoder exit does not verify visual quality. For interactive embeds, preserve reduced-motion behavior and playback controls. For a video export, avoid inserting extra frames to compensate for timeline bugs.

The editable HTML assets are bundled with this skill. Copy them into the project before adapting them; preserve the render function and keep each frame independent of prior calls.
