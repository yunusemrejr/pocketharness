# Score format and arrangement

All `start`, `duration` and `beats` values count quarter notes, even in 6/8. At 120 BPM one quarter note is 0.5 seconds; a 6/8 bar lasts three quarter notes. MIDI pitch 60 is C4, 69 is A4; programs are zero-based. Tracks map to separate melodic channels. Percussion and tempo maps require a DAW or a separate MIDI workflow.

Example short motif; develop it according to the musical request:

```json
{"score":{"bpm":100,"beats":4,"numerator":4,"denominator":4,"tracks":[{"name":"Motif","program":0,"waveform":"triangle","notes":[{"pitch":60,"start":0,"duration":0.8,"velocity":86},{"pitch":64,"start":1,"duration":0.8,"velocity":80},{"pitch":67,"start":2,"duration":0.8,"velocity":90},{"pitch":64,"start":3,"duration":1,"velocity":72}]}]}}
```

Deliver a new folder containing `score.mid`, `score.json`, and `preview.wav`. The WAV is mono 44.1 kHz PCM with short attack/release envelopes and gain reduction only when necessary. Program numbers do not change its oscillator sound. Notes are quantized to 480 ticks per quarter note; JSON records the quantized timing. The preview includes rests through `beats`.

Use one track per instrumental role. Avoid same-pitch overlaps on a track: a MIDI note-off would ambiguously stop a still-held note. Legato across different pitches is valid. Split intentional same-pitch layers across tracks. Keep notes inside score duration; release envelopes reach zero before their note ends.

Harmony: choose chord functions and bass motion, then voice chords within playable register with deliberate common tones and tensions. Rhythm: vary accents and rests, maintain the chosen groove, and use intentional anticipation or syncopation. Arrangement: add density to support form and reserve register space for the melody or dialogue.

For loop delivery, check the final decay and transition to the first beat in playback. For synchronized cues, retain a table of cue seconds and musical beats; `beat = seconds * bpm / 60` applies only to constant tempo. A MIDI file alone cannot prove musical quality; use listening or clearly report that it remains unverified.
