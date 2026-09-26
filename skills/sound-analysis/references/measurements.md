# Measurement interpretation

- Integrated LUFS is a gated loudness measure. Short clips and silence can make it unstable or undefined. A thirty-second sample does not establish program-wide loudness. Full-program delivery requires a full-program scan.
- Sample peak and true peak differ; reconstruction can overshoot between samples. Near-zero peak is a warning, not proof of audible clipping. Inspect waveform shape and listen; flat tops can arise from processing or source synthesis.
- Overall loudness/level statistics combine channels. For asymmetric noise, phase cancellation or channel-specific clipping, explicitly map each channel with FFmpeg (`-map_channel` or `pan`) and compare separately. Downmixing can hide antiphase content.
- Use the spectrum plot to locate persistent hum bands, broadband hiss or transient bursts. Window length trades frequency resolution against time resolution. Plots can rescale their colors; do not compare apparent brightness without matching scale and gain.
- Silence detection needs explicit operational thresholds (level in dB and minimum duration). Leading/trailing events can remain open at a measurement boundary. Do not cut pauses without checking pacing, room tone and word endings.
- Tempo and musical key require a separate estimator or musical analysis. If using librosa or another installed library, record version, window, channel handling and estimator; check half/double tempo ambiguity and non-tonal material by listening. A peak near 440 Hz does not establish a song's key.
- For synchronization, cross-correlate equivalent content, then verify against distinct transients. One delay does not correct clock drift; compare early and late anchors before resampling.

References: [FFmpeg astats](https://ffmpeg.org/ffmpeg-filters.html#astats-1), [loudnorm](https://ffmpeg.org/ffmpeg-filters.html#loudnorm), [silencedetect](https://ffmpeg.org/ffmpeg-filters.html#silencedetect).
