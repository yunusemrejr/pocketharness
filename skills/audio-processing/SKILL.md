---
name: audio-processing
description: "Inspect, transform and validate audio with FFmpeg or SoX: filtering, resampling, channel layouts, loudness, synchronization and output quality; use for audio signal workflows."
---

# Audio processing

Probe the original before choosing filters: streams, codec, sample rate, channel layout, duration, timestamps and existing clipping. Define the deliverable's required format and audible goal. Preserve the source and write a distinct output; a codec conversion cannot restore information already lost.

For native WAV analysis use `pocket kit audio input.wav`; it measures sample peak, RMS, clipping and silence, not integrated LUFS or perceived quality. `pocket kit mix` combines voice/music and timed cue tracks, normalizes/ducks the mix and accepts an explicit duration. `--voice-trim START:END`/`--music-trim START:END` trim source seconds before placement and loudness analysis, `--voice-db DB` adjusts the normalized voice while preserving relative music level, and `--fade-in SEC` adds an opening fade. Check current help for supported controls. Use FFmpeg/SoX or the existing DAW for restoration and unsupported formats; selecting this workflow does not assume those tools are installed.

Use `ffprobe` for probing and FFmpeg filters (`astats`, `ebur128`, `loudnorm`, `showspectrumpic`) for bounded measurements and spectrum images; use FFmpeg directly for WAV extraction or measured two-pass normalization. Pass the interval explicitly with `-ss`/`-t`: default to a bounded sample rather than scanning a whole long program. For long programs and custom effects, run FFmpeg as a background job.

Read [signal pipeline](references/signal-pipeline.md) for sample-rate conversion, filters, channel handling, clipping, PCM and SoX usage. Read [loudness and alignment](references/loudness-alignment.md) for measured normalization, synchronization, delay, drift and verification. Check installed encoder and filter support rather than assuming a particular FFmpeg build includes optional libraries.

Bound every output. `apad` without `pad_dur`/`whole_dur`, `aevalsrc`/`anullsrc`/`sine` without `d=`, and `-stream_loop -1` produce endless streams that fill the disk. Pad with `apad=whole_dur=D` or follow `apad` with `atrim=0:D`, or pass `-t D`. Never "simplify" a closing trim away.

Make stream selection and output parameters explicit. Keep a lossless intermediate when multiple operations would otherwise repeatedly encode a lossy format. Apply transformations in an intentional order, record the actual filter chain and use bounded measurements before processing a large batch. Distinguish peak normalization, perceived loudness, dynamic compression and denoising; they solve different problems.

Inspect resulting metadata, sample counts or duration, clipping and representative playback. Numeric checks do not prove intelligibility or absence of artifacts. If playback is unavailable, say which objective checks passed and leave listening quality unverified. For batches, test silence, short clips, unusual channels and an already loud input before scaling. Deliver files alongside source-to-output mapping and any changed timing, channel or loudness assumptions.

## Reusable editing progression

Discover the source once and save its format/timing with the existing task state. Process a short representative region first, compare the requested change with the source, then apply the accepted chain to the full bounded output. Reuse unchanged intermediates; only repeat downstream checks when their input changed.

Example trim/fade/export, when the installed FFmpeg supports these filters:

```bash
ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,channel_layout -show_entries format=duration -of json input.wav
ffmpeg -nostdin -i input.wav -af "atrim=start=2:end=12,asetpts=PTS-STARTPTS,afade=t=in:d=0.05,afade=t=out:st=9.9:d=0.1" -c:a pcm_s16le -ar 48000 edited.wav
pocket kit audio edited.wav
```

The result is ten seconds with its timeline starting at zero. Choose fade points from the actual trim length, verify channel intent and listen when available. Do not append a filter just because it exists: denoising can remove desired content, gain cannot repair clipped input and resampling must avoid aliasing. For a voice/music cue mix, use separate measured sources and `pocket kit mix out.wav --voice voice.wav --music music.wav --duration D --lufs -14`, then verify the exported audio. Loudness target follows the user's medium; -14 LUFS is one video recipe, not a universal music standard.

Native source-trim/mix example:

```bash
pocket kit mix edited-mix.wav --voice input.wav --voice-trim 2:12 --voice-db -3 --fade-in .05 --duration 10 --lufs -14
pocket kit audio edited-mix.wav
```

Without `--duration`, the native mixer spans all placed tracks/cues. An explicit duration deliberately clips the output, so verify that it preserves the intended ending. Short cues still need timing/peak checks even when they are too short for a full gated loudness measurement.
