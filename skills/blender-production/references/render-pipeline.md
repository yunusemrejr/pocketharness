# Render pipeline

Run a small preview before committing to a sequence. Choose engine, resolution, samples, denoising and device from the scene's required look and available hardware. Measure seconds per frame and peak memory on representative difficult frames; extrapolate duration with headroom. More samples do not fix bad lights or broken geometry.

Blender command-line argument order matters: loading a file can replace settings, and the render action runs with settings already processed. Put output and range settings before the render trigger. [Command-line arguments](https://docs.blender.org/manual/en/5.1/advanced/command_line/arguments.html).

```text
blender --background scene.blend --render-output /absolute/frames/frame_#### --render-format PNG --render-frame 1
```

Use the locally installed version's options. For scripted setup, keep code in a file; pass `--python-exit-code 1` before the script so Python exceptions can fail the job. Avoid loading untrusted blend auto-run scripts. A render subprocess should have a finite budget and write logs separately from final assets.

For animation prefer PNG or EXR sequences so an interrupted render can resume missing frames. EXR supports high-dynamic-range compositing; PNG is convenient for display-ready frames. Preserve alpha intentionally, and document scene-linear versus display transforms. Do not apply a display transform twice during video assembly. Test first, middle and last frames plus the most difficult motion/lighting moments.

Assemble frames with explicit input frame rate, codec, pixel format and audio synchronization; use terminal-video-editing when doing that export. Verify no missing frame numbers, constant image dimensions and expected duration. Keep a lossless master or sequence where future editing matters. Pack or collect required assets, check missing textures on reopening, and report which GPU/CPU and engine actually performed the render.
