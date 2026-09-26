# Loudness and alignment

## Choose the quantity being controlled

Peak amplitude, RMS level, integrated loudness and true peak describe different properties. Raising a quiet file's peak to a fixed threshold does not guarantee equal perceived loudness across clips. Dynamic compression changes the relationship between quiet and loud portions; normalization may instead apply a constant gain. Choose the target from the actual delivery requirement, not a universal social-media preset.

For finite nonzero amplitude x relative to full scale, dBFS = 20 log10(|x|); silence maps to negative infinity. An amplitude gain of g decibels corresponds to multiplication by 10^(g/20). These relations help check calculations, but integrated loudness involves a different measurement process and must not be inferred from one peak sample.

When using FFmpeg `loudnorm` for a fixed asset, measure first with the same stream and preprocessing intended for the output. Supply the measured integrated loudness, true peak, loudness range and threshold to the second pass, plus the reported offset when applicable. Linear normalization can fall back to dynamic processing when its conditions are not met; inspect the report instead of assuming linear mode was honored. Set the output sample rate explicitly. Details are in the [official loudnorm section](https://ffmpeg.org/ffmpeg-filters.html#loudnorm).

Re-measure the rendered file. Silence, very short clips and unusual channel layouts can produce unhelpful or undefined loudness results; do not feed non-finite measurements into a second pass. Keep measured values associated with the precise input and filter chain. Reusing measurements after trimming or remixing invalidates the experiment.

## Distinguish offset from drift

A fixed delay shifts all events by the same amount. Clock mismatch creates a growing discrepancy over time. Compare at least two well-separated landmarks: if offset changes, correcting only the start will not fix the end. For sample rate Fs, a shift of k samples is k/Fs seconds. Express alignment precision in samples or milliseconds, not vague statements that tracks are synchronized.

Cross-correlation can propose a delay for related signals, but repeated beats, silence and differently processed channels can create competing peaks. Verify multiple landmarks and report ambiguity. A transcription timestamp is not automatically a sample-accurate anchor. For video, distinguish audio timestamps, frame presentation times and codec delay before adjusting tracks.

When resampling to correct drift, document the measured ratio and verify beginning, middle and end. Avoid destructive stretching when a timestamp or container interpretation error is the actual cause. After trimming or concatenating, inspect boundaries for clicks and discontinuities; a short fade may be appropriate only if it preserves the requested content.

Deliver original and resulting duration, target and measured loudness, applied delay or ratio, output format and listening checks. If source timing is uncertain, retain that uncertainty rather than reporting an exact synchronization result unsupported by landmarks.
