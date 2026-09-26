---
name: performance-experiments
description: Profile and benchmark CPU, GPU, memory, I/O and application latency to find causal bottlenecks and verify optimizations.
---

# Performance Experiments

State the user-visible performance target and representative workload. Keep correctness checks separate from timing so faster wrong work cannot win.

1. Record hardware, software versions, workload size, concurrency and relevant configuration. Reproduce the slow path before optimizing.
2. Profile to distinguish compute, allocation, locking, network and I/O. Measure completed asynchronous work; enqueue time is not execution time.
3. Compare baseline and candidate under matched conditions, with warmup and repeated trials. Separate setup, compilation and cache effects from steady-state behavior. Randomize order when drift could bias the comparison.
4. Report distributions, counts and uncertainty appropriate to the measurements. Name the population behind p95/p99; do not average percentiles across incompatible runs.
5. Check scaling, memory and quality after the change. Preserve a regression workload that catches the bottleneck rather than asserting a fragile wall-clock threshold in ordinary unit tests.

Example: a GPU kernel launch returns before work finishes. Use the runtime's supported synchronization or timing events around the measured region.

Deliver a reproducible benchmark and a bottleneck-supported explanation. If improvement is within noise, say so and avoid complexity without a demonstrated benefit. Use statistical-experiments for formal treatment-effect or causal population claims.
