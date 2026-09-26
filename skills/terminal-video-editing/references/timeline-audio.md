# Timeline and audio

Build an explicit timeline with source in/out points, destination start times, transitions and audio choices. Use rational frame rate and source timestamps; do not assume 29.97 is exactly 30 or force variable-frame-rate recordings to constant rate without a reason. FFmpeg options apply according to their position; set inputs and outputs deliberately and map required streams. [FFmpeg processing model](https://ffmpeg.org/ffmpeg.html).

For precise filtered trims, pair video trim with setpts=PTS-STARTPTS and audio atrim with asetpts=PTS-STARTPTS. Normalize dimensions, sample aspect ratio, frame rate and audio format before concat filters. Stream-copy cuts can follow keyframes rather than the desired exact visual boundary. Concat demuxing requires compatible streams; different codec parameters call for a normalized intermediate or re-encode.

Crossfades overlap clips, so subtract overlap from total duration. Use audio crossfades intentionally; abrupt audio cuts click. Keep dialogue intelligible, use fades or measured ducking under speech, and avoid clipping when summing sources. amix mixes signals while amerge combines channel streams; they are not interchangeable. Preserve stereo/channel layout unless the destination requires a change.

For two-pass loudnorm, measure integrated loudness, range, true peak and threshold on the actual final mix, then supply measured values in the render pass and verify again. Silence and short material need explicit handling. Target loudness depends on destination; do not hard-code a broadcast target for every web clip. [FFmpeg audio filters](https://ffmpeg.org/ffmpeg-filters.html).

Check sync near the beginning and end to distinguish constant offset from clock drift. Do not use -shortest unless discarding the longer tail is intended. Treat subtitles, alternate language tracks and chapter metadata as explicit deliverables. For specialized denoising or sample-rate diagnosis, consult audio-processing rather than duplicating a full audio workflow.
