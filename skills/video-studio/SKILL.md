---
name: video-studio
description: Make publish-grade motion videos (YouTube, Shorts, explainers, promos, product and educational videos) with narration, generated music, captions, branded call-to-action and real assets, using native pocket kit tools. Read before any video request; covers pipeline, art direction, audio mix, branding rules and the QA gate.
---

# Video studio

A motion video combines timing, story and sound. Inspect the user's artifact need, existing scene/assets and available tools, choose the mode, then run only the relevant pipeline steps. The native `pocket kit` route needs no Node/npm/Python runtime; preserve an existing animation, Blender or editing project when it already fits the task.

## 1. Mode and brief (before any file)

- **Publish mode** follows an explicit public-delivery/platform brief. Choose hook, chapters, captions, end card and thumbnail only when they serve that brief. A public educational or artistic video does not automatically need subscription prompts.
- **Private/prototype mode** applies to a demo, internal clip, test or an unspecified small render: use the requested craft and artifact, without adding a marketing layer. A silent GIF/animation needs no narration/music workflow.
- Write the brief in six lines and keep it: audience; the one thing they get from the video (the promise); platform(s) and aspect (16:9 1920x1080 YouTube, 9:16 1080x1920 Shorts/Reels/TikTok, 1:1 1080x1080); length (as short as it fully delivers); identity the user supplied (name, handle, site, colours, logo); sources for every factual claim.
- **Identity**: preserve supplied or established colors, type, assets and tone. No invented channel, author, handle or logo. Put a supplied handle/watermark/end card where the content and platform need it; fixed placement times and extra calls to action are optional recipes, not requirements.

## 2. Story that keeps people watching

- Hook in the first 3-5 s: the payoff, a surprising fact, or an open question. No logo sting, no "hey guys", no throat-clearing. Never promise what the video does not deliver.
- Then a one-line promise, then chapters. Change something visually every 4-8 s (new layout, camera move, diagram step) so the eye has a new event; hold long enough to read (about 0.3 s per on-screen word, 1.5 s minimum for a diagram).
- One idea per beat; on-screen text is 8 words or fewer and never repeats the narration paragraph. Show the mechanism (a diagram that builds, data that moves) instead of a slide of bullets.
- End on the payoff or the habit, then a 5-15 s end card in publish mode. Leave the lower-right free on 16:9 for platform end-screen elements.
- Numbers and claims must be real and sourced. Unknown figures become labelled placeholders, never plausible fakes.
- Write the script for the ear: short sentences, concrete words, contractions. Blank line between beats. `{TTS|tee tee ess}` shows one form and speaks another for acronyms and brand names.

## 3. Pipeline

Check current command help, tool availability and completed artifacts first. Keep the brief, source revision, measured cues, assets and checks in the existing task state. Reuse an unchanged voice/music/asset output; scene-only changes invalidate previews/render checks, not source audio. Prototype one representative beat, inspect it, then render the low-resolution full preview. Escalate resolution and render the final only after the preview's actual defects are resolved.

```bash
pocket kit theme "topic words" [--seed N]          # palette + font pair derived from the subject; re-roll with --seed
pocket kit asset font "Family" fonts/ --weights 400,700   # local woff2 + css (scenes must not depend on the network)
pocket kit say --setup                              # once: Piper engine + neural voice (~90 MB); then:
pocket kit say narration.wav --file script.txt      # + narration.json/.srt/.captions.html/.cues.css (measured beat and sentence times)
pocket kit music music.wav --style corporate --duration D --seed N     # styles: ambient lofi corporate cinematic tech upbeat
pocket kit sfx whoosh.wav whoosh --seed 3 ; pocket kit sfx tick.wav click
pocket kit mix audio.wav --voice narration.wav --music music.wav --duration D --lufs -14 --at 9.3:whoosh.wav:-2 ...
pocket kit frame scene.html check.png --time 6 --size 1280x720      # lint + a still: look at it
pocket kit video scene.html preview.mp4 --duration D --fps 12 --size 640x360 --audio audio.wav
pocket kit vsheet preview.mp4 sheet.png --n 12                      # the whole video in one image: read it
pocket kit video scene.html final.mp4 --duration D --fps 30 --size 1920x1080 --audio audio.wav
pocket kit vcheck final.mp4 --duration D
```

The full worked example is `examples/capabilities/video/render.sh` (narrated, scored, captioned, with click-timed like/subscribe). Copy `assets/motion.css` from this skill next to your scene; it holds every primitive below.

- **Scenes are timelines in CSS.** Each effect starts at an absolute time: `<h1 class="rise" style="--at:var(--u2-s)">`. `narration.cues.css` defines `--b1-s/--b1-e` (beat start/end) and `--u1-s` (sentence start) from the measured audio, so visuals land on the words. `window.renderFrame(t)` is optional: use it (or Canvas/SVG/WebGL) only when CSS cannot express the motion. Never accumulate frame deltas.
- Word times inside a sentence are estimated from syllables; sentence and beat times are measured. Put big hits on sentence starts; for a single word use `narration.json` word times and check the frame.
- `narration.captions.html` is a ready `.cap` element (phrase spans timed to the voice); paste it into the scene. Restyle `.cap` to match the design. Keep `narration.srt` as the delivery caption file.
- Sizes use `calc(N * var(--u))` with `--u: calc(100vh / 1080)`, so the same scene renders at 640x360 for preview and 1920x1080 for final.
- Long videos: `kit video` handles up to 900 s; render in parts with `--start S --duration D` and join with `ffmpeg -f concat -c copy`. Keep the CPU calm: preview at low resolution and low fps, render the final once.

## 4. Art direction (defeat the slop)

Load the `ai-design-slop` skill too: its tells apply to video frames exactly as to pages.

- Derive the look from the subject: its vocabulary, units, real objects. Run the swap test: change the title, and if the video still works for an unrelated topic it is generic.
- Colour: preserve the project's actual palette. With no established palette, `kit theme` offers a subject-derived starting point. Reserve accent color for meaning and contrast; remove gratuitous gradient washes and glow halos rather than replacing a deliberate brand color.
- Type: use established brand fonts or a subject-appropriate hierarchy, with local files from `kit asset font` when needed. A familiar family is not itself a defect. Monospace suits code/data; avoid choosing it as an unexplained personality cue. At 1080p start near display 110-190, headings 56-96 and body 44-56, then verify actual readability; these are starting ranges, not a reason to reject an existing identity.
- Layout: title-safe margin 5%, one focal point per frame, asymmetry and scale contrast over three identical cards. Change layout between beats; never the same slide three times in a row.
- Motion: entrances 0.5-0.9 s with the expo-out ease in `motion.css`; springs (`.pop`) for objects that have mass; exits faster than entrances; camera pushes 1.03-1.1 scale over 4-8 s (`.cam`); stagger 0.05-0.12 s. Direction means causality (inputs enter left/top, results right/bottom). Nothing bobs or spins without a reason. Text reveals by mask (`.mask > .up`) or highlight (`.mark`), not typewriter.
- Real assets beat drawn stand-ins: `kit asset search "query" --kind image|audio|model|hdri|texture`, `kit asset get URL|polyhaven:ID DIR` (CC0 Poly Haven models/HDRIs/textures, Openverse images/audio with licence; `ATTRIBUTION.txt` is written for you). Animate images with `.cam` (Ken Burns on a `overflow:hidden` frame), cut-outs with `.from-l`, `.pop`. 3D: `kit asset get three DIR` then load a glTF in `renderReady`; WebGL renders in software, so keep models light and the pixel ratio at 1. Credit CC BY assets in the description or end card.
- Blender is only for an existing Blender project (see the blender skills); the native route covers most explainers.

## 5. Sound

- Voice first: neural voice via `kit say`, `--speed .95-1.05`. Listen impossible? Say so; verify with `kit audio` and `vcheck` instead of claiming it sounds good.
- Music serves the voice: pick the style by mood (ambient/cinematic for reflective, lofi for study/chill, corporate/upbeat for product and promo, tech for developer topics). `kit mix` normalises the voice, puts the music 14 LU under it, ducks it a further 8 dB while someone speaks, adds sound effects as accents, normalises the programme to -14 LUFS and holds a -1.5 dBFS ceiling. Silence is a tool: let the hook breathe.
- Source edits can stay native: `kit mix --voice-trim START:END --music-trim START:END --voice-db DB --fade-in SEC` trims before placement/loudness and adjusts balance/opening. Check current help, preserve originals and verify the resulting cue times. Automatic duration spans placed tracks; an explicit `--duration` deliberately selects the ending.
- Sound effects are punctuation: a whoosh on a beat change, a tick on a counter, a chime on a confirmation; at most one per 3 s. Attach each to a visual event (its time comes from the cue file).
- `kit mix` is the native default for cue-sheet mixing. For an existing editor, unsupported source format or a precise effect outside its controls, use a verified FFmpeg/DAW equivalent with explicit duration, channel layout and measured output. Load `audio-processing` for editing; an audio-only task does not require video production.

## 6. Publish layer (publish mode)

- When the brief calls for a call to action, place it after the value has been delivered and keep it small. Existing components `.cta`, `.cta.clicks`, `.bell`, `.thumb` and `.handle` in `motion.css` are optional; use the supplied identity and platform needs.
- Deliverables: `final.mp4` (H.264, yuv420p, BT.709, faststart, AAC 48 kHz), `thumbnail.png` (`kit frame` at the strongest frame, 1280x720: one image, three words at most, high contrast), `captions.srt`, and a short `description.txt` with a keyworded first line, chapter timestamps from the beat cues (`0:00 Intro` ...), asset credits and links the user supplied.

## 7. QA gate: the work is not done until these pass

1. Inspect `kit frame`/`kit video` lint findings in the actual rendered frame. Fix clipping, overlap, illegibility and unintended blank states; inspect style cues in context and preserve a specific intentional identity/state exception. A lint heuristic cannot establish visual quality.
2. `kit vsheet` was read as an image: hook is legible in the first frames, no beat is blank, layouts change, the end card is complete.
3. `kit vcheck final.mp4 --duration D` exits 0: streams present, no black or unintended frozen spans, loudness near -14 LUFS, no clipping.
4. Captions match the audio; names and numbers are right; every claim is sourced.
5. Report what you could not verify (you cannot hear playback; timing inside a sentence is estimated).

Details on motion and pacing: [craft notes](references/craft.md).
