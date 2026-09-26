# Rendering and delivery

The local exporter accepts an HTML file exposing `window.renderFrame(seconds)`:

```bash
node scripts/render.mjs ./title.html ./renders 6 30 1280 720
```

Run from this skill's installed directory (`~/.config/muse/skills/motion-graphics-production` for a user install).

Arguments are input, existing output parent, duration seconds, FPS, width and height. It creates a new folder with numbered PNG frames, H.264 MP4 and a render manifest. It requires an installed Playwright, a Playwright Chromium browser and FFmpeg with libx264. Check these locally; missing dependencies produce an explicit error. It does not install browsers automatically.

The page must fit the requested viewport. The starter uses an SVG viewBox for scalable composition. Each frame is rendered at `index / fps`; frame count is `round(duration * fps)` and the manifest reports the resulting duration. The last frame is before the endpoint; do not add an extra endpoint frame. A single clock should own animated state. Pause autonomous CSS/WAAPI/video animation and replace timer/random-dependent behavior before exporting.

The exporter blocks remote page requests; keep assets local and package them with the source. Use existing background execution for longer or heavier renders. The bundled script caps exports at sixty seconds, sixty FPS and a 1920-pixel dimension. For alpha, HDR or large 3D renders, use an appropriate renderer and codec rather than assuming the MP4 preset preserves them.

After encoding, use `ffprobe` and extracted still frames to verify metadata and inspect representative frames. Review continuous playback for smoothness, typography hold time and audio sync. Add supplied music with explicit FFmpeg stream mappings; retain original duration unless a deliberate trim, loop or fade is part of the edit.

For audio-reactive work, compute an envelope from RMS or band energy, smooth attack/release and derive it from absolute time. Beat grids are estimates until checked against music. Keep text readable through peaks and inspect quiet passages as well as loud ones.

References: [Playwright screenshots](https://playwright.dev/docs/screenshots), [FFmpeg documentation](https://ffmpeg.org/ffmpeg.html).
