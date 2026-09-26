# Signal processing pipeline

## Inspect and specify

Probe only the needed fields before decoding a large asset. For example, `ffprobe -v error -show_streams -show_format -of json input.wav` exposes stream and container information in structured form; select narrower fields for routine batches. See [ffprobe documentation](https://ffmpeg.org/ffprobe.html). Record which stream is used: a video can contain commentary, alternate languages or silent tracks. Container duration and decoded sample duration may differ because of timestamps or codec padding.

Define sample rate, channels, channel layout, codec and target container explicitly. PCM storage is approximately duration × sample rate × channel count × bytes per sample, plus headers. Ten minutes of stereo 48 kHz, 24-bit PCM is about 172.8 MB in decimal units. This estimate bounds intermediate storage before processing.

A sample-rate conversion changes the sampling grid while retaining the intended physical duration. Relabeling the rate without resampling changes speed and pitch. The Nyquist frequency is half the rate; lowering the rate requires removal of frequencies that cannot be represented. Do not present upsampling as recovered detail. For an FFT of N samples, bin spacing is Fs/N; increasing zero padding interpolates the display but does not create additional measured frequency resolution.

## Compose filters intentionally

Treat gain, equalization, denoising, channel mixing, resampling and limiting as distinct operations. Start with the least destructive processing that answers the request. Inspect a representative noisy and quiet segment before applying aggressive denoising to a whole recording. A high-pass filter can remove rumble but also remove useful low-frequency content; choose cutoff from the source rather than a universal preset.

Channel count alone does not describe speaker positions. Inspect the layout and specify the downmix rather than assuming any two channels are stereo. Summing correlated channels can clip, while averaging opposite-polarity channels can cancel important audio. Compare both channels and listen before collapsing them. Preserve a lossless intermediate across multiple edits; repeated lossy encodes accumulate damage.

FFmpeg's `aresample` and audio filters are documented in the [filter reference](https://ffmpeg.org/ffmpeg-filters.html). Confirm optional resampler availability in the installed build. For an installed SoX workflow, inspect `sox --help` and the relevant effect help before writing a chain; `sox input.wav output.wav rate 48000` illustrates a rate conversion. Do not assume SoX is installed or that an effect exists in every build; consult the [packaged SoX manual](https://manpages.debian.org/bookworm/sox/sox.1.en.html).

## Verify the output

Check output stream parameters, duration or expected sample count, non-finite samples where applicable, clipping and channel integrity. Listen to starts, ends, edits and the most processed segment. Compare at matched loudness so louder output is not mistaken for better processing. Save the exact command and input identity with the result; keep original samples available for reversal.
