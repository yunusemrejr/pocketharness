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
        # A local footage clip is part of the same absolute timeline. Its source
        # must seek rather than stay on its first frame while the scene advances.
        footage = out / "footage.mp4"
        run("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", "color=c=red:s=320x240:r=10:d=1",
            "-f", "lavfi", "-i", "color=c=blue:s=320x240:r=10:d=1", "-filter_complex", "[0:v][1:v]concat=n=2:v=1:a=0",
            "-c:v", "libx264", "-pix_fmt", "yuv420p", str(footage))
        footage_scene = out / "footage.html"
        footage_scene.write_text('<!doctype html><style>body{margin:0}video{width:320px;height:240px}</style>'
                                 '<video src="footage.mp4" preload="none" data-render-start=".5" data-render-offset=".25" muted></video>')
        for seconds, blue in (("0", False), ("1.5", True)):
            png = out / "footage-frame.png"
            kit("frame", footage_scene, png, "--time", seconds, "--size", "320x240", "--no-lint")
            pixels = run("ffmpeg", "-v", "error", "-i", str(png), "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1")
            rgb = pixels[(120 * 320 + 160) * 3:(120 * 320 + 160) * 3 + 3]
            assert (rgb[2] > 220 and rgb[0] < 40) if blue else (rgb[0] > 220 and rgb[2] < 40), (seconds, list(rgb))
        footage_scene.write_text(footage_scene.read_text().replace(' muted>', ' loop muted>'))
        kit("frame", footage_scene, out / "loop-frame.png", "--time", "2.5", "--size", "320x240", "--no-lint")
        pixels = run("ffmpeg", "-v", "error", "-i", str(out / "loop-frame.png"), "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1")
        rgb = pixels[(120 * 320 + 160) * 3:(120 * 320 + 160) * 3 + 3]
        assert rgb[0] > 220 and rgb[2] < 40, list(rgb)
        footage_scene.write_text('<video><source src="absent.mp4" type="video/mp4">'
                                 '<source src="footage.mp4" type="video/mp4"></video>')
        kit("frame", footage_scene, out / "fallback.png", "--time", "1.5", "--no-lint")
        footage_scene.write_text('<video><source src="absent.mp4" type="video/mp4"></video>')
        failed = subprocess.run([str(binary), "kit", "frame", str(footage_scene), str(out / "invalid-video.png"),
                                 "--no-lint", "--timeout", "3"], capture_output=True, timeout=10)
        assert failed.returncode == 1 and b"Video failed to decode" in failed.stderr, failed.stderr
        assert not (out / "invalid-video.png").exists()
        missing = out / "missing-image.html"
        missing.write_text('<img src="absent-required.png"><script>window.renderFrame=t=>{}</script>')
        protected = out / "protected.png"
        protected.write_bytes(b"previous completed artifact")
        failed = subprocess.run([str(binary), "kit", "frame", str(missing), str(protected), "--no-lint"],
                                capture_output=True, timeout=90)
        assert failed.returncode == 1 and b"Image failed to decode" in failed.stderr
        assert protected.read_bytes() == b"previous completed artifact"
        missing.write_text('<img src="absent-required.png" data-render-optional><script>window.renderFrame=t=>{}</script>')
        kit("frame", missing, protected, "--no-lint")
        assert protected.read_bytes().startswith(b"\x89PNG")
        model = out / "triangle.gltf"
        (out / "triangle.bin").write_bytes(struct.pack("<9f", -1, 0, 0, 1, 0, 0, 0, 1, 0))
        model.write_text(json.dumps({"asset": {"version": "2.0"}, "buffers": [{"uri": "triangle.bin", "byteLength": 36}],
                                    "bufferViews": [{"buffer": 0, "byteLength": 36}],
                                    "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
                                    "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
                                    "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}))
        report = json.loads(kit("asset", "inspect", model))
        assert report["valid"] and report["triangles"] == 1 and report["vertex_references"] == 3
        # Style names used to be categorical defects even for deliberate owned
        # typography and palette. Render actual DOM text, keeping all measured
        # readability checks active; no downloaded font or network is needed.
        typography = out / "owned-typography.html"
        typography.write_text('''<!doctype html><html lang="en"><style>
html,body{margin:0;background:#101826;color:#fff}
main{padding:36px;display:grid;grid-template-columns:1fr 1fr;gap:20px}
p{margin:0;font-size:28px}i{display:block;width:120px;height:30px;background:linear-gradient(90deg,#6366f1,#ec4899)}
</style><main><p style="font-family:monospace">Cue 01</p><p style="font-family:Inter,sans-serif">Track 02</p>
<p style="font-family:Geist,sans-serif">Timbre 03</p><p style="font-family:'Instrument Serif',serif">Tempo 64</p>
<p style="font-family:system-ui">Measure 07</p><p style="font-family:Arial,sans-serif">Take 08</p><i></i></main>
<script>window.renderFrame=t=>{}</script></html>''')
        clean = subprocess.run([str(binary), "kit", "frame", str(typography), str(out / "owned-typography.png"),
                                "--size", "640x360"], check=True, capture_output=True, timeout=90)
        assert b"no layout or legibility findings" in clean.stderr, clean.stderr
        assert b"source review suggestion" not in clean.stderr and b"default AI font" not in clean.stderr
        defects = out / "readability-defects.html"
        defects.write_text('''<!doctype html><style>
html,body{margin:0;background:#fff;color:#111;font:28px Arial}
.edge{position:absolute;left:-30px;top:30px}
.clip{position:absolute;left:36px;top:80px;width:100px;height:40px;overflow:hidden;white-space:nowrap}
.low{position:absolute;left:36px;top:160px;color:#eee;font-size:24px}
.small{position:absolute;left:36px;top:220px;font-size:5px}
.overlap{position:absolute;left:350px;top:100px}
</style><div class="edge">Cut off</div><div class="clip"><span>Clipped phrase</span></div>
<div class="low">Low contrast</div><div class="small">Unreadable size</div>
<div class="overlap">First phrase</div><div class="overlap">Other phrase</div>
<script>window.renderFrame=t=>{}</script>''')
        broken = subprocess.run([str(binary), "kit", "frame", str(defects), str(out / "readability-defects.png"),
                                 "--size", "640x360"], check=True, capture_output=True, timeout=90)
        for message in (b"cut off by the frame edge", b"clipped by an overflow:hidden", b"legibility floor",
                        b"has contrast", b" overlaps "):
            assert message in broken.stderr, (message, broken.stderr)
        print("media integration: CSS/WAAPI/footage pixels, asset safety, 3D preflight, identity-preserving lint, readability defects, clip edit, H.264/AAC decode and vcheck passed")


if __name__ == "__main__":
    main()
