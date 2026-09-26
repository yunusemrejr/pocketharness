# Narration and sound

## Writing for the ear

- One idea per sentence, 8–18 words. Put the important word at the end of the sentence, where the voice lands.
- Say what the picture shows, in the order it appears, but add what the picture cannot: why it matters, what it replaced, what happens next.
- Avoid parentheses, lists read aloud, and symbols. Spell out numbers the way they should be spoken ("twenty seventeen", "a hundred million").
- Aim for about 3.3–4.3 syllables per second (roughly 140–170 words per minute for typical prose; short plain words can run faster in wpm while sounding relaxed). Dense technical passages go slower; transitions can go faster. `narration_tts` reports the measured rate per scene.
- Leave breaths: 0.3–0.6 s before a scene's narration starts (`narrationOffset`) and about 0.8 s after it ends before the cut.

## Local narration with Piper

```js
tool_search({ names: ["narration_tts"] })
narration_tts({ action: "install", voice: "en_US-ryan-high" })   // once per machine, checksum-pinned
narration_tts({ action: "synthesize", dir: "my-video", fitScenes: true })
narration_tts({ action: "synthesize", dir: "my-video", scenes: ["intro"], speed: 0.95 })
```

`synthesize` writes `public/audio/narration/<scene>.wav` and records `narrationAudio` and `narrationSeconds` in `video.json`. With `fitScenes: true`, scenes shorter than their narration are lengthened. Cues are scene-relative, so visuals keep their internal timing. Re-time cues to the spoken words afterwards.

Pronunciation: TTS guesses at names and acronyms. Pass a `lexicon` to `synthesize` instead of respelling narration text: `narration_tts({action: "synthesize", dir: "my-video", lexicon: {"Vaswani": "Vas-wah-nee", "GPT": "G P T"}})`. The lexicon applies to the spoken text only; `video.json` keeps the display spelling for captions and on-screen text, and the result reports `lexiconEdits` per scene. Re-synthesize only the affected scenes. Keep one project lexicon and reuse it for every synthesis run.

A human recording can replace any scene: put the file in `public/audio/narration/`, set `narrationAudio`, and measure `narrationSeconds` with `media_info`.

## Music bed

`audio_synth kind:"music"` renders a seeded ambient bed sized to the timeline. Direct it:

- `key`/`mode`: minor or dorian for reflective and documentary tones, major for optimistic tones.
- `progression`: four chords, `barsPerChord` 2 for calm, 1 for momentum.
- `bpm` 70–90 under narration. Faster tempos compete with speech.
- `layers`: lower `pulse` and `bell` for dense narration; raise them for visual-only beats. Add `drums` (0.2–0.4, kick on beats 1 and 3 plus eighth hats) for momentum sections and explainer energy; leave it at 0 under dense narration.
- `intensity`: automation points `[seconds, 0..1]` that follow the story arc: low in exposition, rising into the key reveal, resolving at the end. Drums follow the same automation.

The template ducks music under narration windows automatically (`musicVolume` to `musicDuckedVolume`). Measure the result with `video_qa`; adjust the two volumes in `video.json`.

## Sound accents

`audio_synth kind:"sfx"` with `type` whoosh (object crossing frame, transition), riser (tension into a reveal), downlifter (energy draining out, section end), impact (reveal lands), tick (items counting or stepping), pop (UI confirmations, small appearances), chime (conclusion, success). Use one accent per idea, place it at the visual event (`video.json` `audio.sfx: [{src, at, volume}]`), and keep it 6–12 dB under narration. If you can't say which visual event a sound belongs to, delete it.

## Captions and sound-driven visuals

Most viewers meet a video muted first. Keep `captions.enabled` on for explainers and social cuts: captions come from the narration text and its measured length, so re-run `narration_tts` after editing a line and the captions follow. Use `karaoke` style for short social pieces where the active word helps pacing, `chunks` for calmer documentary work. The final render writes `captions.srt` and `captions.vtt`; upload them as platform subtitles instead of relying only on burned-in text. Caption timing is estimated from syllables and punctuation, so check two or three chunks against the audio in the preview.

When music carries a section without narration, let the visuals listen: `AudioSpectrum` reads the actual audio file each frame (align it with `offsetSeconds` to where that audio starts). Tie accents to visible events: a `Glitch` or a `KineticText` word landing on the same frame as its sound accent reads as one gesture.

## Loudness targets

| Target | Integrated | Peak |
| --- | --- | --- |
| Web and social default | -16 LUFS | ≤ -1 dBFS |
| Platforms that normalize (YouTube) | -14 LUFS | ≤ -1 dBFS |
| Narration-only explainer | narration peaks around -3 to -6 dBFS; music 15–20 LU below the voice | |

Adjust `narrationVolume`, `musicVolume` and `musicDuckedVolume` in `video.json`, re-render, and measure with `video_qa`. For standalone audio deliverables, `media_edit action:"normalize"` does measured two-pass loudness normalization.
