#!/usr/bin/env python3
"""Real Chrome/FFmpeg checks; unit tests separately cover dependency failures.

Run: python3 tests/media_integration.py ./pocket [--out build/media-integration]
No network, downloaded fonts, voice engines or Python media packages are needed.
"""
import argparse
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    required = ("ffmpeg", "ffprobe")
    if any(not shutil.which(name) for name in required) or not any(
        shutil.which(name) for name in ("google-chrome", "chromium", "chromium-browser", "google-chrome-stable")
    ):
        parser.error("real media checks require Chrome/Chromium, ffmpeg and ffprobe")
    with tempfile.TemporaryDirectory(prefix="pocket-live-media-") as scratch:
        out = args.out.resolve() if args.out else Path(scratch)
        out.mkdir(parents=True, exist_ok=True)

        def run(*command):
            return subprocess.run(command, check=True, capture_output=True, timeout=90).stdout

        def kit(*command):
            return run(str(binary), "kit", *map(str, command))

        scene = out / "short-animation.html"
        scene.write_text("""<!doctype html><style>
html,body{margin:0;background:#202020}
.box{width:40px;height:40px}#css{background:#ff0000;animation:shift .1s linear}
#waapi{background:#00ffff;margin-top:20px}@keyframes shift{to{transform:translateX(100px)}}
</style><div id="css" class="box"></div><div id="waapi" class="box"></div><script>
document.getElementById('waapi').animate([{transform:'translateX(0)'},{transform:'translateX(100px)'}],{duration:100});
window.renderReady=new Promise(resolve=>setTimeout(resolve,300));
</script>""")
        # Both fill:none animations used to finish before renderReady, making them
        # vanish from getAnimations(). Seek the middle and then backwards to zero.
        for seconds, first in ((".05", 50), ("0", 0)):
            png = out / ("middle.png" if seconds == ".05" else "start.png")
            kit("frame", scene, png, "--size", "320x240", "--time", seconds, "--no-lint")
            pixels = run("ffmpeg", "-v", "error", "-i", str(png), "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1")
            assert len(pixels) == 320 * 240 * 3
            for y, colour in ((20, b"\xff\0\0"), (80, b"\0\xff\xff")):
                positions = [x for x in range(320) if pixels[(y * 320 + x) * 3:(y * 320 + x) * 3 + 3] == colour]
                assert positions == list(range(first, first + 40)), (seconds, y, positions)

        source = out / "source.wav"
        with wave.open(str(source), "wb") as wav:
            wav.setparams((1, 2, 48000, 0, "NONE", "not compressed"))
            wav.writeframes(b"".join(struct.pack("<h", round(5000 * math.sin(2 * math.pi * 997 * i / 48000))) for i in range(48000)))
        mix = out / "mix.wav"
        kit("mix", mix, "--voice", source, "--voice-trim", ".1:.9", "--duration", "1", "--fade-in", ".02", "--fade-out", ".08")
        movie = out / "animation.mp4"
        kit("video", scene, movie, "--duration", "1", "--fps", "10", "--size", "320x240", "--audio", mix, "--no-lint")
        info = json.loads(run("ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(movie)))
        video = next(s for s in info["streams"] if s["codec_type"] == "video")
        audio = next(s for s in info["streams"] if s["codec_type"] == "audio")
        assert (video["width"], video["height"], int(video["nb_frames"])) == (320, 240, 10)
        assert video["codec_name"] == "h264"
        assert audio["codec_name"] == "aac" and abs(float(info["format"]["duration"]) - 1) < .05
        run("ffmpeg", "-v", "error", "-i", str(movie), "-f", "null", "-")
        kit("vcheck", movie, "--duration", "1")
        print("media integration: CSS + WAAPI seek pixels, clip edit, H.264/AAC frames, full decode and vcheck passed")


if __name__ == "__main__":
    main()
