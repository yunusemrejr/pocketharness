---
name: terminal-video-editing
description: "Edit video and synchronized audio through FFmpeg/ffprobe: cuts, joins, transitions, subtitles, mixing, remuxing and efficient MP4/WebM/MKV delivery with measured quality."
---

# Terminal video editing

Probe every source for streams, codecs, duration, frame rate, time base, dimensions, rotation, color metadata and audio layout. Establish the edit timeline and target playback requirements. Preserve source files and record a reproducible edit/filter plan.

Use `ffprobe` for probing and capability checks (`-encoders`, `-filters`), FFmpeg frame extraction for timestamped review samples, and a bounded FFmpeg command for a test SDR H.264/AAC export. Select primary streams explicitly and omit subtitles, chapters and attachments unless they matter; use explicit `-map` options when they do. For motion-graphic sources, use motion-graphics-production and its deterministic exporter.

Read [timeline and audio](references/timeline-audio.md) for frame-accurate edits, transitions, dialogue/music and synchronization. Read [encoding and verification](references/encoding-verification.md) for codec selection, compression, remuxing and validation. Load only the relevant reference. Inspect installed encoders and filters before choosing commands.

Use explicit stream mappings and intentional timing. Stream copy avoids re-encoding when codecs and the requested operation permit it; filtering requires decoding and encoding the affected stream. A smaller lossy encode cannot guarantee unchanged quality. For strict preservation retain the original compressed streams or use a lossless intermediate; for delivery measure an acceptable perceptual tradeoff on representative clips.

Test a short difficult segment before a full encode. Inspect motion, gradients, text, lip sync, transitions and loudness. Validate the final file through probing, decoding and representative playback; successful muxing alone does not prove correct content. Keep editable project/timeline information or commands alongside the final deliverable when useful. State which streams were dropped or transformed and distinguish objective checks from viewing/listening checks that could not run.
