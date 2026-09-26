# Production pipeline

Use the smallest set of artifacts that makes the video reproducible. A six-second effect may need one HTML file and a few storyboard notes; a researched explainer needs its sources, script and cue sheet.

| Step | Concrete artifact | Acceptance |
| --- | --- | --- |
| Message | Claim, audience, factual sources where needed | Each factual claim has evidence; uncertainty is explicit |
| Beats | Cue seconds, visual change, label/narration | The viewer can follow what changed and why |
| Implementation | HTML/SVG/Canvas or existing project scene | Absolute-time rendering; fonts/assets ready; seeded randomness |
| Preview | Short, smaller render and extracted frames | No clipping, unreadable labels, broken transitions or accidental blank frames |
| Audio | Measured local voice/music/effects when requested | Intelligible voice, intentional silences, no clipping, cues aligned |
| Final | Requested video plus editable sources | Metadata and decode checked; inspected frames and playback findings recorded |

A useful beat specification is: `2.0s: reveal the input nodes; 2.8s: propagate a highlighted signal along edges; 4.2s: show the measured output`. Keep continuing objects at consistent positions across beats. Store cue seconds with the content, not in unrelated rendering functions.

Start with `pocket kit video scene.html preview.mp4 --size 640x360 --fps 24 --duration 6`. Use a new output path for each intentional revision. Check one scene or a shorter duration first; do not repeat a full encode for unchanged content. Render the final only after preview defects are addressed.

Delegate independent scene work only when the project is large enough to justify it. Assign one owner to the timeline and shared visual primitives. For a short clip, one implementation and one evidence-based review are usually enough. Additional review rounds need a specific unresolved defect, not a fixed ceremony.
