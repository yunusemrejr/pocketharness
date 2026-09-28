#!/bin/sh
# Whole pipeline for a narrated, scored, captioned video: theme fonts, neural voice with timing,
# generated music, mix, poster, render, and QA. Needs Chrome + FFmpeg; the first run downloads the voice
# (`pocket kit say --setup`) and two Google Fonts. Run from anywhere.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
here="$root/examples/capabilities/video"
out=${1:-"$root/build/capabilities/video"}
pocket=${POCKET_BIN:-"$root/pocket"}
mkdir -p -- "$out/fonts"
cp "$root/skills/video-studio/assets/motion.css" "$out/"

"$pocket" kit asset font "Familjen Grotesk" "$out/fonts" --weights 400,700
"$pocket" kit asset font "Literata" "$out/fonts" --weights 400,500,600
"$pocket" kit say --voices | grep -q . && ! "$pocket" kit say --voices | grep -q none || "$pocket" kit say --setup
"$pocket" kit say "$out/narration.wav" --file "$here/script.txt"

# Cue times come from the measured narration, never from guesses.
cue() { sed -n "s/.*--$1:\([0-9.]*\)s.*/\1/p" "$out/narration.cues.css" | head -1; }
word() { awk -v w="\"w\": \"$1\"" '/"s":/{gsub(/[^0-9.]/,"",$2); s=$2} index($0,w){print s; exit}' "$out/narration.json"; }
dur=$(awk '/"duration"/{gsub(/[^0-9.]/,"",$2); print $2 + 1.6; exit}' "$out/narration.json")
printf ':root{--like-at:%ss;--sub-at:%ss;}\n' "$(word like)" "$(word subscribe)" > "$out/cta-times.css"

"$pocket" kit music "$out/music.wav" --style corporate --duration "$dur" --seed 4
"$pocket" kit sfx "$out/whoosh.wav" whoosh --duration .55 --seed 3 --lowpass 2800
"$pocket" kit sfx "$out/tick.wav" click --duration .08
"$pocket" kit sfx "$out/chime.wav" chime --duration .9 --freq 1046
"$pocket" kit mix "$out/audio.wav" --voice "$out/narration.wav" --music "$out/music.wav" --duration "$dur" --lufs -14 \
  --at "$(cue b2-s):$out/whoosh.wav:-2" --at "$(cue b3-s):$out/whoosh.wav:-2" --at "$(cue b4-s):$out/whoosh.wav:-2" \
  --at "$(cue u4-s):$out/tick.wav" --at "$(cue u5-s):$out/tick.wav" --at "$(cue u6-s):$out/tick.wav" \
  --at "$(word like):$out/chime.wav:-6" --at "$(word subscribe):$out/chime.wav:-6"

awk -v f="$out/narration.captions.html" '/<!--captions-->/{while((getline l < f)>0) print l; next} {print}' "$here/scene.html" > "$out/scene.html"
"$pocket" kit frame "$out/scene.html" "$out/poster.png" --time "$(cue u5-s)" --size 1920x1080
"$pocket" kit video "$out/scene.html" "$out/demo.mp4" --duration "$dur" --fps 30 --size 1920x1080 --audio "$out/audio.wav"
"$pocket" kit vcheck "$out/demo.mp4" --duration "$dur"
"$pocket" kit vsheet "$out/demo.mp4" "$out/sheet.png" --n 12 --cols 4
