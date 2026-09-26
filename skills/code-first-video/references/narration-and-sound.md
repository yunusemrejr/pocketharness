# Narration and sound

Keep narration concrete and readable aloud. Measure the actual recording before locking cue seconds; estimated speaking rates are planning aids, not synchronization evidence. Leave room for breaths and for the visual explanation to land.

PocketHarness does not bundle TTS, speech alignment or automatic captions. Use a supplied recording or an existing local TTS executable after reading its installed help. Keep display text and pronunciation substitutions separate. Do not download a voice or contact a paid service merely because a guide mentions one. If narration is not required, a silent animation or sparse procedural sound can be the right result.

Use the procedural-audio skill for `pocket kit wav`, `sfx` and `audio` syntax. Seed generated sounds, use an envelope to avoid abrupt edges, and attach each effect to a specific visual event. A sound effect is punctuation, not a substitute for meaningful motion. Keep music sparse under speech and listen to the actual mix; no automatic ducking is implied.

Prepare a single soundtrack whose timing matches the video, then pass `--audio soundtrack.wav` to `pocket kit video`. Measure source duration and allow for the requested video length. Explicitly trim, fade, loop or pad in an existing audio editor/FFmpeg workflow when necessary. Do not assume a shorter track will repeat or a longer narration will fit.

For captions, use available aligned timestamps or hand-checked timing. Estimated word timing is not forced alignment. Verify names, line breaks and reading pace against playback. Deliver a requested subtitle file only after checking it against the actual audio.

Native peak/RMS checks detect some signal faults but do not measure perceived quality or integrated LUFS. Use the [QA measurements](qa-loop.md) and the destination's actual loudness specification when compliance matters. Report whether you listened and whether pronunciation, balance and synchronization were verified.
