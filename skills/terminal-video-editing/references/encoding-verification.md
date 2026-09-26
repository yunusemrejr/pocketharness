# Encoding and verification

Probe with `ffprobe -v error -show_format -show_streams -of json INPUT`. MP4 is a container, not an encoder. A compatibility-oriented SDR delivery often uses H.264 video, AAC audio and yuv420p, but this drops alpha and cannot preserve HDR by simply changing metadata. Inspect the destination and source before adopting it.

For a compatible source and container, remux selected streams with stream copy; this preserves their encoded packets while container metadata/timestamps may change. Re-encoding is required for scaling, grading or burned-in subtitles. Avoid repeated lossy intermediate encodes. Retain a source/master when exact preservation matters. [Stream copy and mapping](https://ffmpeg.org/ffmpeg.html).

Example starting point for already-SDR video with a desired first audio track, if libx264 exists:

```text
ffmpeg -n -i INPUT -map 0:v:0 -map 0:a:0? -c:v libx264 -preset medium -crf 20 -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart OUTPUT.mp4
```

This is a trial preset, not a quality guarantee. It excludes extra tracks and subtitles intentionally; adjust mapping to the user's requirements. Preserve aspect ratio and rotation behavior. CRF values are not comparable across encoders. Hardware encoding may improve speed at a bitrate/quality cost; measure a difficult segment. AV1/HEVC can reduce size when playback support and encoding budget permit. WebM commonly uses VP9/AV1 and Opus; inspect actual muxer support.

For a size budget, approximate total bitrate as 8*target_bytes/duration_seconds, then subtract audio and container overhead; use codec-appropriate rate control. Decode-check with `ffmpeg -v error -i OUTPUT -f null -` and inspect expected streams, duration, resolution, color and file size. Compare moving detail, dark gradients and small text. SSIM/VMAF can support matched-reference evaluation but cannot alone prove perceived quality or audio correctness.
