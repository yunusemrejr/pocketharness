---
name: inference-serving
description: Optimize and validate model-serving throughput, KV-cache behavior, batching, quantization and request isolation under memory and latency constraints.
---

# Inference Serving

Define the workload before optimizing: prompt/output lengths, concurrency, hardware, precision and quality requirements. Separate provider-reported usage from local estimates; unknown cache or pricing fields stay unknown.

1. Establish a correctness baseline and measure time to first token, inter-token latency, throughput, memory and errors. Separate warmup from steady state and cached from uncached requests.
2. Identify the bottleneck using available profiling and request traces. Change batching, context limits, precision or scheduling one at a time; avoid tuning unrelated settings together.
3. Check KV-cache capacity and request isolation. Cache keys must include the state that affects computation; similar-looking prompts do not establish a reusable exact prefix.
4. For quantization or alternate kernels, compare quality on representative and sensitive cases, not only speed. Record supported shapes/dtypes and fallback behavior.
5. Test cancellation, overload, long prompts, mixed-length requests and clean recovery from memory pressure. Keep queue bounds and admission policy explicit; throughput gains do not excuse unbounded tail latency.

Example: a warmed identical prompt can make a route look fast while most production requests miss the cache. Report both distributions and the actual hit denominator.

Deliver reproducible workload/configuration, measured resource and quality changes, and limits of the evidence. Avoid inventing provider capabilities from model-family names.
