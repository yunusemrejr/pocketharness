# Production pipeline

Each phase produces an artifact in the project directory. The next phase starts only when the artifact exists and its gate passes. Keep the artifacts short and concrete. They are working documents, not reports.

| Phase | Artifact | Gate |
| --- | --- | --- |
| Research | `research.md`: claims with sources, dates, names, numbers | Every factual claim in the script traces to a note; uncertain claims are cut or hedged |
| Narrative | `script.md`: beats with narration lines | Read aloud at a comfortable pace (≈3.8 syllables/s) fits the target length ±10%; each beat has one idea; the opening states why the viewer should care within 10 seconds |
| Storyboard | `storyboard.md` (format below) | Every beat has a visual system that changes over time; no beat is "text on a background"; on-screen text ≤ 8 words per beat |
| Scene spec | `video.json` scenes, seconds, cues, props | `video_project action:"check"` has no errors |
| Implementation | `src/scenes/*.tsx`, new primitives | Type-safe, deterministic, components read data from props |
| Frame review | contact sheets per round | Every scene reviewed and passed (see QA loop) |
| Motion review | scene previews | Pacing, transitions and motion meaning pass |
| Audio | narration, music, sfx in `public/audio` | Durations fit, cues re-timed, balance measured |
| Sync review | preview of whole video or long ranges | Visual events land on their narration words (±4 frames) |
| Final | `out/final-*/final.mp4` + `video_qa` report | No QA errors; contact sheet reviewed; findings addressed or justified |

## Storyboard format

```markdown
## 03 · attention-weights (22s)
Claim: each word decides how much every other word matters to it.
Visual system: a sentence as a row of tokens; arcs grow between tokens, thickness = weight; a matrix builds beside it from those arcs.
Motion: arcs grow from the focus token outward (reading direction = causality); matrix cells fill as each arc lands; focus moves to the next token.
On-screen text: "Attention = learned relevance" (appears at the matrix reveal)
Narration: "Every word looks at every other word ... and decides how much each one matters."
Cues: tokens 0.4, focus 2.0, arcs 3.1, matrix 8.5, label 12.0
Transition in/out: continues from 02's token row (same positions); fades to 04.
```

Keep the same objects across adjacent beats when the idea continues. Continuity reads as one argument; hard resets read as slides.

## Using subagents

Delegate when work is separable and large enough to pay back the overhead:

- **Research** for documentary or historical topics: a research subagent gathers sourced facts into `research.md`.
- **Independent review**: after the first full render, a reviewer with vision reads the contact sheets, watches previews and reports defects against the QA checklist. A second viewer catches what the author has stopped seeing.
- **Parallel scene implementation** for long videos: once primitives, theme and `video.json` are fixed, disjoint scene components can be written in parallel. Keep one owner of `video.json` and shared primitives to avoid conflicting edits.

Do the storyboard and the final judgement yourself. For short videos (under ~45 seconds), work solo and run one independent review at the end at most.

## Time and cost discipline

- Render stills (seconds) many times; render previews (tens of seconds) per scene; render the final once or twice.
- Review a contact sheet first. Open individual full-size stills only for scenes that need detail.
- Re-render only changed scenes: `video_render mode:"stills" scene:"<id>"`.
- Install narration voices once. Synthesize narration only after the script is final, then re-synthesize changed scenes by id.
