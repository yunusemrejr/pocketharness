# Video QA

Inspect the result produced by the renderer. A zero exit status does not establish readability, motion quality or speech intelligibility.

```bash
ffprobe -v error -show_entries format=duration:stream=index,codec_name,width,height,r_frame_rate,nb_frames -of json final.mp4
ffmpeg -v error -i final.mp4 -f null -
mkdir -p frames
ffmpeg -v error -i final.mp4 -vf fps=2 -frames:v 12 frames/frame-%03d.png
```

These commands require installed FFmpeg/ffprobe. Read representative PNGs with the image-capable `read` tool. Sample both settled states and transition boundaries; a regular two-frame-per-second sample can miss a short defect. Preview continuous playback when a player is available and state when only frames were inspected.

Check:

- Text stays inside its safe area, contrast is sufficient, and labels are readable at the delivery size.
- Each beat has one clear focal change, enough reading time and continuity with its neighbours.
- The first/last frames, reveal boundaries and loop seam are intentional; no flicker or accumulated-time drift.
- The observed frame count, dimensions and duration match the requested timeline within one frame.
- Speech and visual cues align; sound does not cut off unexpectedly or hide the voice.

For a WAV, `pocket kit audio soundtrack.wav` reports supported signal measurements. Peak/RMS measurements are not integrated loudness. If loudness compliance is requested, use the installed FFmpeg loudness filter and the delivery specification:

```bash
ffmpeg -hide_banner -i final.mp4 -filter_complex ebur128=peak=true -f null -
```

Listen when playback is available. Do not infer pleasant sound, correct pronunciation or good balance from a clean waveform alone. Fix observed defects, rerun the affected checks, and deliver the source and final artifact with measured properties and remaining limitations.

Primary references: [ffprobe](https://ffmpeg.org/ffprobe.html), [FFmpeg ebur128](https://ffmpeg.org/ffmpeg-filters.html#ebur128).
