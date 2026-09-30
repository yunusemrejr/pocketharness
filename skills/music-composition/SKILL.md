---
name: music-composition
description: Compose original melodies, harmony, rhythm and arrangements; create editable MIDI scores and audible sketches, including music timed to video. Use for writing music, cue sheets and orchestration rather than recognizing existing songs.
---

# Music composition

Use the user's mood, instrumentation, references and duration to make musical choices. Establish a tonal center or deliberate atonality, meter, tempo, motif and form. Start with a short coherent phrase; develop repetition and contrast before adding parts. Keep bass, chord voicing and melody intentional rather than generating unrelated scale notes.

Discover available render/edit tools and existing score/project first. A short native sketch can finish directly; a longer scored production may need the existing DAW, a verified MIDI renderer or targeted arrangement/listening review. Keep the motif, tempo/cue contract and completed renders in the existing task state; changed orchestration need not repeat dataset/tool discovery or an unchanged score review.

Render sketches natively with `pocket kit wav` (chords `C4+E4+G4`, simultaneous tracks separated by `|`, drum hits `K`/`S`/`H`, a `saw>` prefix per track) and check levels with `pocket kit audio`; load the procedural-audio skill for the full syntax.

```bash
pocket kit wav sketch.wav "C4+E4+G4:2 A3+C4+E4:2 | saw> C2:1 C2 A1 A1 | K:.5 H S H K H S H" --bpm 96 --gain .2
pocket kit audio sketch.wav
pocket kit music bed.wav --style ambient --duration 20 --seed 17
```

The first command renders an explicit editable note-string sketch; keep that source alongside the WAV. `kit music` supplies a deterministic native background score when that suits the brief, not a substitute for intentional melody/arrangement or proof of realistic timbre. Render a short sample first, audition it, then extend duration and mix only when needed. A music-only request does not require narration, captions, a video CTA or an end card.

Turn the explicit score into type-1 MIDI, editable JSON and an audible WAV sketch using available local MIDI tooling (a MIDI library plus a simple software renderer, with no model or soundfont required). Read [score format and arrangement](references/score.md) before constructing the score data. The sketch format supports constant tempo, eight tonal tracks and two-minute sketches. The renderer does not invent the musical content or render realistic instruments.

For picture, map cue points to seconds, then quarter-note beats; choose tempo and phrase lengths to support the edit. Record intentional pickups, holds, endings and loop seams. For longer work or tempo maps, use an available DAW/MIDI library with the same timing contract.

Audition the result for rhythm, voice leading, register, balance and ending. Check note-off timing, overlapping pitches, count-in and exact duration. Program numbers are preserved in MIDI, while the built-in preview uses sine/triangle oscillators. Render through an available soundfont or DAW when instrument timbre matters. Deliver editable score/MIDI with the audio, and distinguish a sketch from a mixed production.
