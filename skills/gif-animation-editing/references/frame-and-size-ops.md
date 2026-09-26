# Frame and size operations

GIF has no stream copy: trimming, resizing, or reordering always decodes and re-encodes. Keep the source file untouched, work from copies, and record exact commands with tool versions. Verify installed options first — flag names differ across gifsicle, ImageMagick, and FFmpeg releases.

## Probing

`gifsicle --info INPUT.gif` reports frames, dimensions, delays, loop count, and per-frame disposal; `identify -format "%f: %wx%h %n frames\n" INPUT.gif` (ImageMagick) gives a one-line summary; `ffprobe -v error -show_streams -of json INPUT.gif` shows how FFmpeg sees the container. When tools disagree on frame count, suspect zero-delay or disposal-dependent frames — inspect visually rather than trusting one counter.

Establish the budget from the destination: messaging apps, avatars, and documentation each impose size and dimension caps. Approximate GIF size from frames × dimensions × palette richness; animation with photographic gradients costs far more than flat graphics at the same settings.

## Trimming, selecting, and reordering frames

With gifsicle, select frames by index (`INPUT.gif '#0-29'` keeps the first 30) and delete the rest, or build a new file from selected ranges. Adjust per-frame delays (`--delay`) and the loop count (`--loopcount`) explicitly — source delays rarely match the destination's timing needs. Coalesce optimized frames (`convert INPUT.gif -coalesce`) before any operation that assumes full frames (cropping, compositing, resizing in ImageMagick); re-optimize afterward.

Reordering is selection in a new order; verify motion continuity on the new seams, since disposal methods from the original order can ghost across reordered frames. When in doubt, coalesce first, reorder, then re-optimize.

## Resizing and cropping

Scale with gifsicle (`--scale` or explicit `--resize`) for simple jobs, or FFmpeg's `scale` filter for precise dimensions and aspect control. Preserve aspect ratio unless distortion is intended; pad or crop deliberately for fixed-size slots (avatars, banners). Crop before scaling to save pixels, and keep dimensions even where downstream video tooling requires it.

Resizing animated GIFs resamples every frame — test text legibility and edge shimmer on the hardest frames. Downscaling high-detail animation without palette care produces muddy output; pair dimension changes with the palette strategy below.

## Palettes, dithering, and compression

GIF allows 256 colors per frame (global or local palettes). Reduce with gifsicle (`--colors`) or FFmpeg's two-pass palette workflow (`palettegen` then `paletteuse`): the two-pass route analyzes actual frame colors and consistently beats one-pass quality at the same size. Dithering (ordered, Floyd–Steinberg via `paletteuse` options) trades grain for banding — choose per content: flat graphics want no dithering, gradients want it.

gifsicle optimization levels (`-O1` through `-O3`) trade encode time for size via frame differencing and transparency optimization; lossy GIF compression (`--lossy`) trades small artifacts for large savings on photographic content. Combine: fewer colors + modest lossy + `-O3` usually beats any single lever. Measure size and inspect motion, gradients, and text after each change — one variable at a time.

## Frame-rate reduction

Dropping frames (every Nth frame) shrinks files fast but chops motion; prefer it for low-motion content and pair with delay adjustment to preserve total duration. For smooth motion, reduce dimensions or colors before touching frame rate. Verify duration and loop feel on the actual render — frame math on paper misses perceptual judder.

## Primary references

Check the documentation for the installed version when flags matter. These are reference entry points, not permission to install.

- https://www.lcdf.org/gifsicle/man.html
- https://ffmpeg.org/ffmpeg-filters.html
- https://imagemagick.org/script/command-line-processing.php
