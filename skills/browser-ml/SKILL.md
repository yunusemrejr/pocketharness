---
name: browser-ml
description: Running ML models in the browser — ONNX Runtime Web, transformers.js, TensorFlow.js, WebNN, model conversion & quantization, WebGPU vs WASM backends, latency/size budgets, and private/offline inference patterns. Use when adding inference to a web app or fixing model load/runtime issues in browsers.
---

# Browser-Based ML

## Pick the runtime by task

| Task | Best bet |
|---|---|
| Small LLMs/chat/summarize (0.5B–8B) | **transformers.js** (ONNX, WebGPU, streaming, KV-cache, built-in tokenizer) |
| Classification / OCR / segmentation / audio | **ONNX Runtime Web** (WebGPU/WASM backends, most accurate export tooling) |
| Older mobile stacks / legacy TF models | TensorFlow.js (WebGL/WebGPU) |
| Hardware NPU on Windows/Edge/Samsung | **WebNN** (`navigator.ml` — ONNX in TF.js or standalone; still spreading, ALWAYS have fallback) |
| Python-in-browser (PyTorch, pandas) | Pyodide — heavy (~10 MB runtime), only when users need Python itself |

Rule of thumb: **start from an ONNX export** — it's the common denominator all three runtimes consume.

## Model supply pipeline

1. Export HF model to ONNX: `optimum-cli export onnx --model <id> --opset 17 out/` (or `transformers.js`-friendly: use their HF conversion pipeline; many small models are already ONNX-quantized on the Hub under `Xenova/*`/`onnx-community/*`).
2. **Quantize before shipping:** fp32 → **int8 dynamic** (usually −4× size, −1–2% quality) or fp16. `onnxruntime` quant tools or optimum `--quantize`. int8 is the default web target.
3. Shrink headroom: `onnxsim` (constant folding) — often −20% for no cost.
4. Inspect: node counts, op coverage (WebGPU's opset has gaps — fall back per-op), `netron.app` for visual diff before/after.
5. Budget: **< 30 MB first load** for casual users; > 100 MB needs explicit opt-in + progress UI. LLMs: 0.5B int8 ≈ 500 MB? No — 0.5B int8 ≈ 600 MB is wrong; think in params: 1 param ≈ 1 byte int8 → 8B model ≈ 8 GB, too big for most web. Realistic web-LLM today: **0.5B–4B int8/fp16, 0.5–4 GB, desktop + strong mobile only**; otherwise remote inference.

## Runtime wiring

```js
import { pipeline, env } from "@huggingface/transformers";
env.allowLocalModels = false;                 // or serve from your CDN
const gen = await pipeline("text-generation", "onnx-community/Qwen2.5-0.5B-Instruct-GGUF",
  { device: "webgpu", dtype: "q4f16" });       // wasm backend auto-fallbacks
for await (const out of gen("Explain X in one sentence.",
  { max_new_tokens: 128, do_sample: false })) { /* stream */ }
```

- **WebGPU vs WASM:** WebGPU ≈ 10–50× faster on desktop; availability ~90%+ on evergreen Chrome/Edge/Safari 26+, patchy on mobile. Feature-detect (`navigator.gpu`) and fall back to WASM (slower but universal) — build the UX to survive 10× slower.
- **WASM multithreading needs COOP/COEP header pair** on every response (same as wasm-c-cpp) — OR stay single-threaded and accept ~2×.
- Cache aggressively: models are immutable — serve ONNX bytes from CDN with `Cache-Control: public, max-age=31536000, immutable`; runtime downloads a single ~few-MB `.wasm` — `StreamingResponse`/range requests help mobile.
- Lazy load: `import("@huggingface/transformers")` inside the code path that needs it; never in the main bundle.
- Privacy is the selling point: state clearly what runs locally ("nothing leaves your device") and keep it true — no telemetry-shaped fallback to server.

## Latency engineering

- Streaming tokens: `text-generation-pipeline` with `streamer`/`for await` — first token in < 1–2 s or users bail; prefill dominates, so **short system prompts matter 10× more in-browser than on a server**.
- Batching: run sequential inference for 1 item (browsers rarely batch); for list-y tasks, chunk + `await` interleave or queue in a worker to keep UI alive.
- Keep inference in a **Worker** (WebGPU context + wasm off the main thread = zero jank); transfer `Float32Array`/Tensor data by reference.
- KV cache: transformers.js handles it; if using raw ONNX session, manage past-key-values in-memory and cap `max_length` hard.

## Failure modes seen in the wild

- Model 404s in production: Hub path case-sensitivity, or CDN stripped `.onnx` MIME → silent `instantiate` fail. Serve with correct `application/octet-stream`.
- Works in Chrome, dead in mobile Safari: no WebGPU → ensure WASM fallback path is actually tested, not just written.
- OOM on mid-range phones: fp16 when int8 was available, or 7B when 3A (MoE) was intended.
- "Inference is slow and the page stutters": model loaded in the main thread. Move to worker.
- Eval gap: web-quantized int8 can degrade OCR small fonts 5–10% — re-evaluate on YOUR data after quantization, never assume.