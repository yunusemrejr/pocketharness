# Native sound cues

From the repository root:

```sh
POCKET=./pocket sh examples/capabilities/audio/render.sh /tmp/pocket-audio-demo
```

This makes original cues and a stereo score, edits a recording, and mixes a finished programme without audio libraries, samples, Python or FFmpeg:

| File | Duration | Construction |
| --- | ---: | --- |
| `chime.wav` | .8 s | 660 Hz tone, damped bell partials and a release envelope |
| `whoosh.wav` | .6 s | Seeded noise, low-pass filter, rising/falling envelope |
| `impact.wav` | .4 s | Downward pitch sweep plus filtered noise |
| `motif.wav` | 2.25 s | Sequential notes and a rest at 100 BPM |
| `whoosh-edited.wav` | .42 s | Trim `.08:.5` of the source, fade both edges, deliver at -18 LUFS |
| `score.wav` | 4 s | Deterministic lofi score, stereo 48000 Hz |
| `mix.wav` | 4 s | Full score plus a placed chime, -14 LUFS and -1.5 dBFS ceiling |

The script analyzes each output. Generated files stay in the chosen output directory. Repeated runs with the same settings and binary produce identical samples. Use `pocket kit sfx --help` for controls.

For independent validation when FFmpeg is installed:

```sh
ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,duration -of default=noprint_wrappers=1 /tmp/pocket-audio-demo/chime.wav
ffmpeg -v error -i /tmp/pocket-audio-demo/chime.wav -f null -
```

Sample peak/RMS and periodic pitch are useful checks, not a substitute for listening. Keep headroom when mixing cues into an app or video.

`kit mix` also edits existing recordings: `--voice-trim START:END` and `--music-trim START:END` select source seconds before placement, `--voice-at`/`--music-at` position them, and `--fade-in`/`--fade-out` shape the programme. `--voice-db` adjusts the normalized voice level; `--music-db` is relative to that voice. Without `--duration`, all placed tracks determine the output length. A supplied duration explicitly crops or pads the programme.
