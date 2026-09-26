# Sampling and evidence

- Probe once before decoding. Extract a few evenly spaced stills (for example six across the interval) for a coarse survey, then refine with dense stills around events of interest. Read the returned images, not just filenames.
- FFmpeg scene detection (`select='gt(scene,0.4)',showinfo` or `scdet`) returns approximate candidates. The threshold is a sensitivity score, not a probability. Sample before and after candidates; slow dissolves may be missed.
- The tools report seconds relative to source presentation start. For nonzero or negative container timestamps, retain ffprobe start_time. Do not convert variable-frame-rate timestamps with `frame = time * nominal_fps`; use decoded presentation timestamps for exact work.
- Build a shot log with start/end estimates, actual sampled timestamps, visible action, audio evidence and confidence. A person leaving the sampled frame is not proof they left the scene.
- Compare equivalent crop, orientation and time in two sources. Align audio/video before comparing changes; resizing and color conversion alter pixel metrics. Metadata, pixels and semantic judgments are different evidence.
- Audio analysis measures signals, not words. If speech is required, use an actually available transcription tool/model, retain language and timestamps, and check uncertain names against audio. Do not install large model weights or upload private recordings without task authorization.
- More samples reduce gaps but do not prove complete coverage. For fast actions, lip sync or motion smoothness, inspect a short continuous segment. State when only still frames were available.

References: [ffprobe documentation](https://ffmpeg.org/ffprobe.html), [FFmpeg scdet](https://ffmpeg.org/ffmpeg-filters.html#scdet).
