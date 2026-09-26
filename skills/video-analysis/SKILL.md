---
name: video-analysis
description: Analyze recorded video through timestamped frames, scene candidates, visual events and audio evidence. Use for footage summaries, shot logs, continuity checks or comparisons; use terminal-video-editing for transformations.
---

# Video analysis

Identify the question, source file and interval. Use `ffprobe` to probe streams, dimensions, rotation, color, frame rate and duration. Metadata supports technical claims; understanding the action requires inspecting decoded frames or playback.

Extract frames with FFmpeg for a coarse survey (a few evenly spaced stills), then dense stills around events; open the actual images with an available image/vision tool. Preserve requested and decoded timestamps. Use FFmpeg scene detection (`scdet`/`select`) to locate candidate cuts, then inspect both sides: flashes and camera motion can also trigger the detector. For audio evidence, use FFmpeg loudness/level measurements and representative listening.

Read [sampling and evidence](references/sampling.md) for variable frame rate, shot logs, comparison and speech handling. Never describe a frame as viewed if only its metadata was read. Missing vision, playback or transcription is an evidence limit, not a reason to invent events.

Deliver the requested summary or analysis with source timestamps and confidence. Distinguish visible observation, audible/transcribed evidence and inference. Exact counts and event boundaries require denser sampling or continuous review. Keep conclusions within the sampled coverage; preserve uncertainty about offscreen or intervening activity.
