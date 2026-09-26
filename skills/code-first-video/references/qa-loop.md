# QA loop

Review is a loop: render, look, write down defects, fix, re-render the changed scenes, and look again. Stop only when a round finds no defect of severity *fix*. Budget three to five rounds for a short video; that is normal, not failure.

## Frame review (every round)

`video_render mode:"stills"` returns `contactSheet`. Open it with `read`. For each labelled frame, write one line: scene, verdict (pass or fix), and the concrete defect. Open full-size stills for scenes with small text or dense diagrams.

| Defect | Look for | Typical fix |
| --- | --- | --- |
| Clipping / overflow | Text or shapes cut at edges or inside boxes | Reduce size, wrap earlier, enlarge the container, respect the safe area |
| Weak hierarchy | Eye doesn't know where to land; two things equally loud | Enlarge the subject, mute the secondary layer, remove an element |
| Bad typography | Tiny labels (< 26 px), long lines, orphans, mixed fonts, tight leading | Use the type scale; shorten copy; one line per idea |
| Empty composition | Subject small in a big void; everything hugging one edge | Scale up the subject, recenter, use the thirds |
| Overcrowding | More than ~7 distinct elements competing | Split the beat or build progressively |
| Excess text | Sentences on screen; narration duplicated as captions | Cut to ≤ 8 words; let the visual carry it |
| Inconsistent spacing | Gutters and margins change between scenes | Use `space()` and shared layout components |
| Poor contrast | Grey on grey, thin strokes on busy backdrop | Raise ink contrast, thicken strokes, quiet the backdrop |
| Unreadable diagram | Crossing edges, labels overlapping nodes, no legend for encodings | Re-layout, fewer nodes, label directly, state the encoding once |
| Mid-transition frame looks broken | Half-faded clutter, overlapping scenes | Adjust exit timing so outgoing content clears first |

Frames sampled at 60% of each scene show settled states. Also sample early frames (`times`) to check entrances, and the last 10 frames of scenes to check exits.

## Motion review

Render `video_render mode:"preview" scene:"<id>"` and inspect it. Extract 8–12 frames around transitions and reveals with `video_frames`, then view them in sequence.

- Does each motion answer what changed? Remove motion that doesn't.
- Is order readable (stagger), and does the eye have one focal change at a time?
- Is there time to read each reveal before the next begins?
- Do transitions preserve continuity (same objects, same positions) where the idea continues?
- Are easings consistent (ease-out entrances, quicker exits), with no linear robotic slides?
- Any jitter, popping or flicker? Check determinism: seeded randomness, no time-based state.

## Audio review

- `video_qa` gives integrated loudness (target -16 LUFS for web, -14 for platforms that normalize), peak (≤ -1 dBFS) and loudness range. It also flags silence gaps and narration windows that are barely audible.
- Narration dominates: music sits about 15–20 LU under the voice while narration plays. The template ducks music automatically; verify it.
- Listen-check proper nouns and acronyms in narration; fix with a `narration_tts` lexicon (never by respelling `video.json` narration text, which captions and on-screen text share), then re-synthesize those scenes.
- Watch caption pace: `video_project check` warns past ~24 characters/s. Viewers read ~17–20; shorten the line or lengthen the scene instead of flashing chunks.
- Silence longer than about 1.5 s mid-video feels broken unless it is a deliberate beat under strong visuals.

## Sync review

- For each scene, compare cue seconds with where the key word falls in the narration audio. Visual events should land on or just before their word (0–4 frames early reads as intentional, late reads as lag).
- After changing narration, re-check every downstream scene: `narration_tts` may lengthen scenes (`fitScenes`), which shifts absolute times.

## Final QA

`video_qa path:"out/final-*/final.mp4" dir:"<project>"`. Fix all errors. Treat warnings as questions you must answer: fix them or state why they are intended. Then open the QA contact sheet and do one last full frame review. Deliver with a short note covering duration, resolution, loudness, known limitations and where the sources and storyboard live.

## Independent review

For videos longer than about 45 seconds, ask a reviewer subagent with vision to read the final contact sheets and preview frames against this checklist and the storyboard. Give it the paths and the checklist, not your opinion. Treat its findings as defects to triage, not as approval.
