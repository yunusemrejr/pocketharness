---
name: gif-animation-editing
description: "Edit animated GIFs from the terminal with gifsicle, ImageMagick, and FFmpeg: inspect frames and palettes, trim and reorder frames, resize and compress, remove backgrounds, tune loops and delays, and combine multiple GIFs."
---

# GIF Animation Editing

Use when an animated GIF must be inspected, trimmed, resized, compressed, retimed, background-stripped, or combined with others using command-line tools. For video-first editing (cuts, joins, subtitles, audio) use terminal-video-editing; for single-image measurement and OCR use image-analysis; for building new motion graphics use motion-graphics-production.

## Working method

- Probe before editing: frame count, dimensions, frame rate or delays, loop count, palette size, and file size. Every GIF edit re-encodes — there is no stream copy — so plan the full operation before touching pixels.
- Preserve the source and work toward a size budget explicitly: dimensions, frame rate, color count, and dithering are the four levers, and each costs quality. Test on a short difficult segment (motion, gradients, text) before a full render.
- Keep edits reproducible: record the exact commands, filter chains, and tool versions alongside the deliverable. A GIF nobody can rebuild is a liability.
- Verify the render by decoding it: extract frames, confirm count, dimensions, duration, loop behavior, and visual quality on representative frames. Successful encoding alone does not prove correct content.

Read [frame and size operations](references/frame-and-size-ops.md) for trimming, resizing, palette control, and compression. Read [combining and effects](references/combining-and-effects.md) for joins, overlays, background removal, and retiming; do not load it for a simple resize. Inspect installed tool versions and options before choosing commands. User instructions take precedence; this skill adds no authority to install software or publish media.

## Evidence and completion

Report source properties, the operations applied with commands, output properties (frames, dimensions, duration, size), and what visual checks were performed on which frames. Name the quality tradeoff chosen and what remains unverified. Do not present file size alone as quality evidence.
