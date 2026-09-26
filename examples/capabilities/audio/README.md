# Native sound cues

From the repository root:

```sh
POCKET=./pocket sh examples/capabilities/audio/render.sh /tmp/pocket-audio-demo
```

This makes four original mono PCM16, 44100 Hz files without audio libraries, samples, Python or FFmpeg:

| File | Duration | Construction |
| --- | ---: | --- |
| `chime.wav` | .8 s | 660 Hz tone, damped bell partials and a release envelope |
| `whoosh.wav` | .6 s | Seeded noise, low-pass filter, rising/falling envelope |
| `impact.wav` | .4 s | Downward pitch sweep plus filtered noise |
| `motif.wav` | 2.25 s | Sequential notes and a rest at 100 BPM |

The script analyzes each output. Generated files stay in the chosen output directory. Repeated runs with the same settings and binary produce identical samples. Use `pocket kit sfx --help` for controls.

For independent validation when FFmpeg is installed:

```sh
ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,duration -of default=noprint_wrappers=1 /tmp/pocket-audio-demo/chime.wav
ffmpeg -v error -i /tmp/pocket-audio-demo/chime.wav -f null -
```

Sample peak/RMS and periodic pitch are useful checks, not a substitute for listening. Keep headroom when mixing cues into an app or video.
