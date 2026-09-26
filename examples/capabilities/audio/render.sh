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
for audio_file in "$audio_out/chime.wav" "$audio_out/whoosh.wav" "$audio_out/impact.wav" "$audio_out/motif.wav"; do
    "$pocket_bin" kit audio "$audio_file"
done
