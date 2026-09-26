---
name: procedural-audio
description: Generate music beds and sound effects procedurally in Python (numpy) for videos, apps and games; seeded chord-progression ambience with drums and intensity automation, and designed whooshes, risers, downlifters, impacts, ticks, pops and chimes, plus mixing and loudness discipline. Use when a project needs original music or sound without samples, stock audio or generative AI audio.
---

# Procedural audio

For video projects, call `audio_synth` (discover it with `tool_search`). It runs [the synthesizer](scripts/synth.py) against the project and writes `public/audio/<name>.wav`:

```js
audio_synth({ dir: "my-video", kind: "music", key: "D", mode: "dorian", bpm: 80,
  progression: ["i", "VII", "VI", "VII"], intensity: [[0, 0.3], [40, 0.8], [70, 0.4]] })
audio_synth({ dir: "my-video", kind: "sfx", type: "impact", name: "reveal-hit" })
```

Outside a video project, run the script directly: `python3 scripts/synth.py spec.json out.wav`, using the spec format in its header.

Direct sound like picture:

- Decide the emotional arc first (curious → tense → resolved), then express it as `intensity` automation and chord motion. Constant intensity sounds like a loop.
- Under narration, keep tempo 70–90 bpm, low `pulse`/`bell` layers, `drums` at 0, and a dark bed (little energy above 2 kHz). Speech intelligibility lives around 1–4 kHz.
- For momentum without narration, add `drums` (kick on beats 1 and 3, eighth hats) and match the video's beat-synced motion to the same bpm (`pulse(frame, fps, bpm)` in the Remotion template).
- Use sound effects as punctuation for specific visual events, one per idea, 6–12 dB under the voice: whoosh, riser and downlifter for transitions, impact for reveals, tick and pop for counting and UI, chime for conclusions.
- Every render is seeded. Change `seed` for a different arrangement with the same character.

Verify what you cannot hear directly. `audio_analyze` measures loudness, peaks and silence and can render a spectrum image; check that low-frequency energy does not dominate and that nothing clips. For finished videos, `video_qa` measures the full mix. Numbers support judgement; listen when a listener is available.

For MIDI scores and note-level composition, use `music_compose` and the music-composition skill. For analysis of existing recordings, use sound-analysis.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
