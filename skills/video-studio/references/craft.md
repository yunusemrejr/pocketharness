# Craft notes

## Pacing by length

| Length | Beats | Notes |
| --- | --- | --- |
| 15-30 s (Shorts, promo) | 3-5 | Hook in 2 s, one idea, captions always on, end on the payoff, tiny CTA |
| 1-3 min | 4-8 | Hook, promise, 3 chapters, recap, end card; a visual event every 4-6 s |
| 4-10 min | chapters of 45-90 s | Re-hook at each chapter (question or surprising number), chapter cards, music arc from `kit music` (it builds, breaks down near 60%, resolves) |

Speaking rate for planning: about 2.5 words per second at `--speed 1`. Trust the measured `narration.json`, not the estimate.

## Timing recipes

- Sentence starts are the strongest cues. `--at:var(--u4-s)` for the element the sentence names, `calc(var(--u4-s) + .3s)` for its detail.
- For a word inside a sentence use `narration.json` word `s`, then `kit frame --time` at that value to check.
- Leave 0.4 s after the last word before the next beat's exit; exits run on wrappers: `class="out-fade" style="--out:calc(var(--b3-s) - .5s)"`.
- Beats are `.beat` sections with `--at` and `--len`; they toggle visibility only, so entrances inside them start at absolute times.

## Motion vocabulary in `motion.css`

`.fade .rise .drop .from-l .from-r .pop .zoom .blur-in .wipe .grow-x .grow-y`, `.mask > .up` line reveal, `.stagger` (children step by `--gap`, start at `--from`), `.cam` push/drift, `.drift` ambient loop, `.mark` highlight, `.pulse`, `.draw` (SVG path with `pathLength="1"`), `.count` (`--to`, `--unit`), `.cap`, `.cta`, `.handle`. Duration: `--dur`. Add a new keyframe when a motion recurs; keep timing outside keyframes.

## Diagrams that explain

Build a diagram in reading order: axes, then data, then the highlighted result. Use `.draw` for paths, `.grow-y` for bars, `.count` for figures. Encode with position and length before colour. A diagram that appears all at once is a slide.

## Vertical (9:16)

1080x1920, keep text out of the bottom 20% and right 15% (platform UI), captions larger (44-60 px) and centred at 60-70% height, one idea per 3-4 s, first frame already a hook.

## Loops and GIF-like clips

Make time-periodic effects (`.drift`) use a period that divides the clip length; render with `--duration` equal to one period; check the seam by extracting frame 0 and the last frame with `kit frame`.

## Performance

Frames are captured with software rendering. Avoid `backdrop-filter`, very large blurs, and hundreds of animated elements; a 1080p frame is normally 50-150 ms. Preview at 640x360, 12 fps; render the final once, at night if it is long. WebGL scenes cost more: lower the model detail or the resolution.
