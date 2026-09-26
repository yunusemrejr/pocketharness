#!/bin/sh
# Run from anywhere. Existing Chrome and FFmpeg are optional host executables.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
out=${1:-"$root/build/capabilities/motion"}
pocket=${POCKET_BIN:-"$root/pocket"}
mkdir -p -- "$out"
"$pocket" kit sfx "$out/chime.wav" chime --duration 1 --gain .22
"$pocket" kit sfx "$out/whoosh.wav" whoosh --duration .7 --gain .16
"$pocket" kit sfx "$out/click.wav" click --duration .12 --gain .2
ffmpeg -hide_banner -loglevel error -nostdin -y -i "$out/chime.wav" -i "$out/whoosh.wav" -i "$out/click.wav" \
  -filter_complex '[1:a]adelay=1900:all=1[b];[2:a]adelay=4000:all=1[c];[0:a][b][c]amix=inputs=3:normalize=0,apad' \
  -t 6 "$out/soundtrack.wav"
"$pocket" kit audio "$out/soundtrack.wav"
scene="$root/skills/motion-graphics-production/assets/capability-demo.html"
"$pocket" kit frame "$scene" "$out/poster.png" --time 4.8 --size 960x540
"$pocket" kit video "$scene" "$out/demo.mp4" --duration 6 --fps 24 --size 960x540 --audio "$out/soundtrack.wav"
ffprobe -v error -show_entries stream=codec_name,width,height,r_frame_rate,duration,nb_frames -of json "$out/demo.mp4"
