# Combining and effects

## Concatenating GIFs

Normalize first: same dimensions, same frame rate (or explicitly converted delays), compatible palettes. Then append with gifsicle (multiple inputs in order) or FFmpeg's `concat` filter after normalization. Mismatched dimensions produce a canvas sized to one input with the other cropped or offset — decide the canvas deliberately (pad, crop, or scale each input) instead of accepting defaults.

Verify the joint: total duration equals the sum of parts, the loop point flows sensibly, and no input's disposal method bleeds into the next segment. Coalescing inputs before concat and re-optimizing after avoids most seam ghosts.

## Side-by-side and grid compositions

GIF has no native tiling: compose frames with `hstack`/`xstack` filters or ImageMagick `+append`/`montage` per frame set, then encode the composed frames as one animation. All inputs must share frame count and timing for a clean composite — resample shorter inputs (duplicate or freeze frames) explicitly rather than letting the shortest input truncate the result.

Composites multiply pixels and size; budget dimensions and colors for the combined canvas from the start. Label each source's contribution in the edit notes so the composite stays rebuildable.

## Overlays, captions, and watermarks

Burn text or graphics with per-frame compositing (ImageMagick composite over coalesced frames, or FFmpeg `drawtext`/`overlay` filters). Keep captions legible: sufficient size, contrast outline or backing plate, and on-screen duration that survives loop timing. Verify text rendering on the actual frames — font availability and encoding differ across machines, and small GIF text degrades fast under palette reduction.

Overlays change every frame they touch, which defeats frame differencing and grows the file; bound overlay duration and area, and re-measure size after adding them.

## Background removal

True background removal on animation means per-frame transparency: coalesce, then key out the background color range (fuzz-matched transparency) or apply a mask sequence, then set disposal to restore-background so moving subjects do not smear. Flat solid backgrounds key cleanly; gradients, shadows, and compression noise key badly — inspect edges on the hardest frames and expect manual tolerance tuning.

Transparency in GIF is binary (fully on or off per pixel): semi-transparent edges become halo or jaggies. Mitigate with matte-matched edges when the display background is known, or accept the tradeoff deliberately. Verify against both light and dark display backgrounds when the destination varies.

Treat "AI background removal" tools as external services with their own quality and privacy terms: never upload sensitive or personal footage without explicit authorization, and verify their output frame by frame — automated masks flicker across frames.

## Loops, delays, and playback feel

Set the loop count explicitly (`--loopcount=0` for infinite where supported, or an exact count) and per-segment delays for pacing: holds on key frames, faster motion passages. A seamless loop needs matching first/last frames — crossfade or ping-pong (forward then reversed) constructions hide the joint when a hard loop would jump.

Verify playback feel in a real renderer at the destination size:loop timing that reads well frame-by-frame can still judder or rush when played. Check total duration, loop transition, and text readability at speed before delivery.

## Verification checklist

- Frame count, dimensions, duration, and loop count match the plan.
- Representative frames inspected: motion peaks, gradients, text, transparency edges, concat seams.
- File size within budget with the quality tradeoff recorded.
- Source preserved; commands and tool versions recorded for rebuild.

## Primary references

- https://www.lcdf.org/gifsicle/man.html
- https://ffmpeg.org/ffmpeg-filters.html
- https://imagemagick.org/script/command-line-processing.php
