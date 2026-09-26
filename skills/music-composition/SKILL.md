---
name: music-composition
description: Compose original melodies, harmony, rhythm and arrangements; create editable MIDI scores and audible sketches, including music timed to video. Use for writing music, cue sheets and orchestration rather than recognizing existing songs.
---

# Music composition

Use the user's mood, instrumentation, references and duration to make musical choices. Establish a tonal center or deliberate atonality, meter, tempo, motif and form. Start with a short coherent phrase; develop repetition and contrast before adding parts. Keep bass, chord voicing and melody intentional rather than generating unrelated scale notes.

Turn the explicit score into type-1 MIDI, editable JSON and an audible WAV sketch using available local MIDI tooling (a MIDI library plus a simple software renderer, with no model or soundfont required). Read [score format and arrangement](references/score.md) before constructing the score data. The sketch format supports constant tempo, eight tonal tracks and two-minute sketches. The renderer does not invent the musical content or render realistic instruments.

For picture, map cue points to seconds, then quarter-note beats; choose tempo and phrase lengths to support the edit. Record intentional pickups, holds, endings and loop seams. For longer work or tempo maps, use an available DAW/MIDI library with the same timing contract.

Audition the result for rhythm, voice leading, register, balance and ending. Check note-off timing, overlapping pitches, count-in and exact duration. Program numbers are preserved in MIDI, while the built-in preview uses sine/triangle oscillators. Render through an available soundfont or DAW when instrument timbre matters. Deliver editable score/MIDI with the audio, and distinguish a sketch from a mixed production.
