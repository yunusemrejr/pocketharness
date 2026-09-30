#!/bin/sh
# Pure native audio example. POCKET may point at a newly built binary.
set -eu
pocket_bin=${POCKET:-pocket}
audio_out=${1:-audio-output}
mkdir -p "$audio_out"
"$pocket_bin" kit sfx "$audio_out/chime.wav" chime --freq 660 --gain .28
"$pocket_bin" kit sfx "$audio_out/whoosh.wav" whoosh --duration .6 --seed 17 --gain .22
"$pocket_bin" kit sfx "$audio_out/impact.wav" impact --duration .4 --gain .3
"$pocket_bin" kit wav "$audio_out/motif.wav" "C5:.5 E5:.5 G5:1 R:.25 G5:.5 C6:1" --bpm 100 --wave sine --gain .22
# Trim the noisy lead-in, shape both edges, and set a reproducible delivery level.
"$pocket_bin" kit mix "$audio_out/whoosh-edited.wav" --voice "$audio_out/whoosh.wav" --voice-trim .08:.5 \
    --fade-in .025 --fade-out .08 --lufs -18 --ceiling -1.5
# A short cue must not truncate its longer music bed when no duration was supplied.
"$pocket_bin" kit music "$audio_out/score.wav" --style lofi --duration 4 --seed 17
"$pocket_bin" kit mix "$audio_out/mix.wav" --music "$audio_out/score.wav" --at "1:$audio_out/chime.wav:-6" --fade-out .4
for audio_file in "$audio_out/chime.wav" "$audio_out/whoosh.wav" "$audio_out/impact.wav" "$audio_out/motif.wav" \
                  "$audio_out/whoosh-edited.wav" "$audio_out/score.wav" "$audio_out/mix.wav"; do
    "$pocket_bin" kit audio "$audio_file"
done
