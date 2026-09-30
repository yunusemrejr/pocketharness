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
"$pocket" kit music "$out/music.wav" --style tech --duration 6 --seed 7
"$pocket" kit mix "$out/soundtrack.wav" --music "$out/music.wav" --duration 6 --fade-in .08 --fade-out .5 \
  --at "0:$out/chime.wav:-4" --at "1.9:$out/whoosh.wav:-6" --at "4:$out/click.wav:-6"
"$pocket" kit audio "$out/soundtrack.wav"
scene="$root/skills/motion-graphics-production/assets/capability-demo.html"
"$pocket" kit frame "$scene" "$out/poster.png" --time 4.8 --size 960x540
"$pocket" kit video "$scene" "$out/demo.mp4" --duration 6 --fps 24 --size 960x540 --audio "$out/soundtrack.wav"
"$pocket" kit vcheck "$out/demo.mp4" --duration 6
ffprobe -v error -show_entries stream=codec_name,width,height,r_frame_rate,duration,nb_frames -of json "$out/demo.mp4"
