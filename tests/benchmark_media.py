#!/usr/bin/env python3
"""Paired native DSP/export benchmark, with correctness evidence and no API calls.

python3 tests/benchmark_media.py --before /path/to/old/pocket --after ./pocket --out build/media-benchmark
Use --workload music or --workload video to investigate one workload without repeating the other.
Run on an otherwise idle host. These are native kit timings, not LLM task/token benchmarks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--workload", choices=("all", "music", "video"), default="all")
    parser.add_argument("--note", action="append", default=[], help="record an observed environmental limitation with the raw results")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    binaries = {"before": args.before.resolve(), "after": args.after.resolve()}

    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def timed(command):
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        start = time.perf_counter()
        result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=180)
        elapsed = time.perf_counter() - start
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        return {"command": command, "wall_s": elapsed,
                "cpu_s": after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime,
                "report": result.stdout.strip()}

    def paired(command):
        rows = {phase: [] for phase in binaries}
        for repeat in range(args.runs):
            # Alternating order limits systematic warm-cache and background-load bias.
            for phase in (("before", "after") if repeat % 2 == 0 else ("after", "before")):
                rows[phase].append(timed(command(phase)))
        rows["medians"] = {phase: {metric: statistics.median(r[metric] for r in rows[phase])
                                   for metric in ("wall_s", "cpu_s")} for phase in binaries}
        return rows

    results = {"method": "sequential paired runs; order alternates; median wall and waited child CPU time",
               "workload": args.workload,
               "environment_notes": args.note,
               "scope": "native kit commands; LLM token/tool/coordination metrics are not applicable",
               "binaries": {phase: {"path": str(path), "sha256": digest(path)} for phase, path in binaries.items()},
               "music": {}}
    for style in (("lofi", "corporate", "cinematic", "tech") if args.workload != "video" else ()):
        rows = paired(lambda phase: [str(binaries[phase]), "kit", "music", str(out / f"{style}-{phase}.wav"),
                                     "--style", style, "--duration", "60", "--seed", "7"])
        rows["wav_sha256"] = {phase: digest(out / f"{style}-{phase}.wav") for phase in binaries}
        rows["identical_pcm16"] = len(set(rows["wav_sha256"].values())) == 1
        assert rows["identical_pcm16"], f"{style}: synthesis output differs from baseline"
        results["music"][style] = rows
        print(style, json.dumps(rows["medians"]), "identical PCM:", rows["identical_pcm16"], flush=True)

    if args.workload == "music":
        (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        return

    scene = out / "scene.html"
    scene.write_text("""<!doctype html><style>
html,body{margin:0;background:#172b38}#box{width:200px;height:200px;background:#ff8030}
</style><div id="box"></div><script>window.renderFrame=t=>{
document.getElementById('box').style.transform=`translate(${(Math.sin(t)*.5+.5)*500}px,${(Math.cos(t)*.5+.5)*200}px)`
};</script>""")
    video = paired(lambda phase: [str(binaries[phase]), "kit", "video", str(scene), str(out / f"{phase}.mp4"),
                                 "--duration", "3", "--fps", "12", "--size", "960x540", "--no-lint"])
    hashes = {}
    for phase in binaries:
        path = out / f"{phase}.mp4"
        info = json.loads(subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(path)],
                                         capture_output=True, text=True, check=True, timeout=30).stdout)
        stream = next(s for s in info["streams"] if s["codec_type"] == "video")
        assert (stream["codec_name"], stream["width"], stream["height"], int(stream["nb_frames"])) == ("h264", 960, 540, 36)
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"],
                             capture_output=True, check=True, timeout=90).stdout
        assert len(raw) == 36 * 960 * 540 * 3
        hashes[phase] = hashlib.sha256(raw).hexdigest()
    video["decoded_rgb_sha256"] = hashes
    video["identical_decoded_video"] = len(set(hashes.values())) == 1
    assert video["identical_decoded_video"], "decoded animation differs from baseline"
    results["video"] = video
    print("video", json.dumps(video["medians"]), "identical pixels:", video["identical_decoded_video"], flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
