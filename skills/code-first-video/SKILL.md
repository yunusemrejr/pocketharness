---
name: code-first-video
description: Direct and produce explainer, documentary or motion-design videos entirely in code (Remotion React/TypeScript, SVG, Canvas, optional Three.js, local TTS narration, procedural music and FFmpeg), with no stock footage or generated images. Use for any request to make, script, storyboard, animate, narrate, render or review a video; it sequences research, narrative, storyboard, master timeline, implementation and a mandatory visual/audio QA loop.
---

# Code-first video direction

Treat the video as software whose output is judged by eye and ear. The failure mode to avoid is a slideshow: text cards with fades. Every beat must show an idea working, a visual system the viewer watches change, not a sentence to read.

Move through the phases in order and do not skip a gate. Read [production pipeline](references/production-pipeline.md) for the artifacts, gates and when to use subagents. Scale ceremony to the job: a 20-second loop needs a short storyboard and one review pass; a three-minute documentary needs research notes, a script, a full storyboard, per-scene reviews and an independent review.

1. **Research → narrative.** Gather facts with sources before writing. Write for the ear: one idea per sentence, concrete nouns, about 3.3–4.3 spoken syllables per second.
2. **Storyboard before code.** For each beat: the claim, the visual metaphor, what moves and why, on-screen text (≤ 8 words), duration. Read [visual language](references/visual-language.md) to turn abstract concepts into visual systems, choose primitives, set typography and use motion to carry meaning.
3. **Master timeline.** `video_project action:"init"` scaffolds a Remotion project whose `video.json` owns scene order, seconds, narration, scene-relative cues, music and sound. Narrative, data and timing live there; components only render. Never hard-code timing that narration depends on.
4. **Implementation.** Build reusable scene components from the primitives, parameterised by props from `video.json`. Load the remotion-video skill for code patterns and determinism rules.
5. **Representative-frame review.** `video_render mode:"stills"` returns a labelled contact sheet. Open it with `read` and critique it hard, then fix and re-render. Repeat until every scene passes. Read [QA loop](references/qa-loop.md) for the defect checklist.
6. **Motion review.** `video_render mode:"preview"` per scene or range. Check pacing, easing, transitions and whether motion explains anything. Sample frames around transitions with `video_frames`.
7. **Narration and audio.** `narration_tts` voices each scene and writes measured durations back into the timeline. `audio_synth` creates the music bed and sparse sound accents. Read [narration and sound](references/narration-and-sound.md).
8. **Sync review, final render, final QA.** Re-time cues to the narration's key words, then `video_render mode:"final"` and `video_qa` with `dir` set. Fix every error and review the contact sheet before delivery.

See the [worked example](assets/example-attention/README.md) for a complete small project and the defects its review rounds caught.

A successful render, a passing `video_qa` or a clean type check is never evidence that the video looks or sounds good. Only viewed frames, watched motion and measured, listened-to audio are.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
