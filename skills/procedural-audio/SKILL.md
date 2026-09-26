---
name: procedural-audio
description: Generate original UI sounds, transitions, impacts, chimes and short note sequences with native pocket kit commands. Seeded noise, pitch sweeps, envelopes and filters need no libraries or assets. Use for sound cues in apps, games and videos.
---

# Procedural audio

Use `pocket kit sfx` for designed cues and `pocket kit wav` for a melody. Both write mono PCM16 WAV files atomically and reject invalid arguments before replacing an output. Create the output directory first.

```bash
mkdir -p public/audio
pocket kit sfx public/audio/reveal.wav chime --freq 660 --gain .28
pocket kit sfx public/audio/transition.wav whoosh --duration .6 --seed 17 --gain .22
pocket kit sfx public/audio/hit.wav impact --duration .4 --gain .3
pocket kit sfx public/audio/laser.wav laser --freq 1800 --end-freq 120 --duration .35
pocket kit wav public/audio/motif.wav "C5:.5 E5:.5 G5:1 R:.25 G5:.5 C6:1" --bpm 100 --wave sine --gain .22
pocket kit audio public/audio/reveal.wav
```

`sfx OUT.wav PRESET` accepts `click`, `chime`, `laser`, `whoosh`, `impact`, `tone`, or `noise`. Start with a preset, then adjust only the controls the design needs:

- `--duration SEC`: .001–60 seconds. Default depends on the preset.
- `--freq HZ`, `--end-freq HZ`: start/end frequency of an exponential pitch sweep. Setting only `--freq` makes a steady tone. Set both for a sweep.
- `--gain 0..1`: amplitude, default .3. Leave headroom when combining cues.
- `--noise 0..1`: mix between the tone and seeded white noise.
- `--attack SEC`, `--release SEC`: fade lengths, 0–60 seconds. Overlapping fades reduce the peak; shorten them when shortening a preset. Endpoints remain zero.
- `--lowpass HZ`: one-pole low-pass filter; 0 bypasses it.
- `--seed UINT32`: repeatable noise, default 1; zero is valid.
- `--rate N`: integer sample rate 8000–96000 Hz, default 44100. Frequencies/filter cutoff must be below .49 times the sample rate.

`wav OUT.wav "NOTE:DURATION ..."` accepts note names (`A4`, `C#5`, `Eb3`), frequencies (`440`), and rests (`R`). Separate events with spaces, tabs, commas or newlines. Durations are seconds unless `--bpm 1..1000` is present, then they are beats. An omitted duration is .25. Supported waves: `sine`, `square`, `saw`, `tri`. A render is limited to 60 seconds and 4096 events. This command produces a sequential melody; it does not interpret chords or MIDI.

Match the cue to the action. Use a brief click for direct input, a chime for a confirmed result, a whoosh for motion and an impact for a reveal. Under speech, keep cue levels low and use the low-pass filter to reduce competition with the voice. Avoid adding an identical cue to every event.

Verify each output with `pocket kit audio FILE.wav`. It reports duration, format, sample peak, RMS, clipping, approximate periodic pitch and time in blocks quieter than −50 dBFS. It checks the original channels without averaging away opposite-phase signals. Pitch is a rough estimate for periodic sounds; it is not a music transcription or perceptual quality score. RMS is not LUFS. Listen when playback is available.

Analysis accepts WAV PCM8/16/24/32 and float32, 1–32 channels, 8000–192000 Hz, up to 64 MiB. Other encodings can be converted with an installed FFmpeg. Use FFmpeg's `adelay`/`amix` when the project needs layered cues or a video soundtrack; check the final mix too. Native generation and analysis require no FFmpeg installation.

The runnable example is `examples/capabilities/audio/render.sh`. It generates four cues with fixed settings and analyzes them using only Pocket.
