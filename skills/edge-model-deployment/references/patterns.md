# Edge Model Deployment: patterns and examples

## Feasibility first
Peak memory includes weights, activations, scratch arena, runtime, input buffers, stacks and concurrently live outputs. A rough lower bound for quantized weights is `parameter_count * bits / 8`; scales, zero points, packing and alignment add overhead. Flash capacity is not available RAM. For MCU targets confirm exact chip variant, PSRAM availability, supported kernels and firmware toolchain. An Arduino board may be a tiny MCU or a substantially different system.

Build a matrix: board/OS, runtime version, supported operator set, dtype, accelerator/delegate, fallback behavior, memory and measured latency. Raspberry Pi generations and accelerator accessories differ. Intel/AMD CPU ISA and GPU runtimes differ by model and driver. Apple CPU/GPU/Neural Engine paths have distinct operator and deployment constraints. Never claim a delegate accelerates the full graph without inspecting partition/fallback reports.

## Quantization and inputs
For affine quantization, `x ≈ scale * (q - zero_point)`. Calibration must represent deployment ranges and rare meaningful inputs; fitting on the test set contaminates evaluation. Check per-channel versus per-tensor scaling, saturation, accumulator width and unsupported mixed-precision operations. Reproduce resize, channel order, normalization, audio sample rate and feature windowing exactly. A model can be numerically correct yet useless because preprocessing shifted.

## Streaming and operations
Bound sensor buffers, define dropped-frame policy and use monotonic timestamps. Separate acquisition, inference and presentation so backpressure is visible. Watch thermal throttling and power limits over sustained runs, not only one warm inference. Report cold start, warm latency distribution, memory high-water mark and quality change versus the reference. Include missing sensor data, reboot and low-memory behavior.

Use golden input/output fixtures and small operator-level comparisons to locate conversion errors. Benchmark actual hardware before claiming speed or battery life; desktop simulation only validates part of the pipeline. Ship model/version/preprocessing metadata together with a rollback path. Optimize the measured bottleneck: smaller tensors and fewer transfers may outperform a more exotic accelerator kernel.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://developers.google.com/edge/litert/microcontrollers/get_started
- https://onnxruntime.ai/docs/execution-providers/
- https://developer.apple.com/documentation/coreml
- https://docs.espressif.com/projects/esp-dl/en/latest/
