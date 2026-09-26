---
name: llm-systems-engineering
description: "Engineer LLM architectures, attention kernels, quantization and fine-tuning with quality and memory validation."
---

# LLM Systems Engineering

Use for model internals and training/inference transformations. Pair with inference-serving for deployment operations and model-evaluation for outcome measurement.

## Working method

- Establish architecture, tokenizer, attention layout and exact engine support.
- Account for weights, KV cache, activations and training states separately.
- Preserve a reference implementation and held-out task suite.
- Measure quality, throughput and latency under matched workloads.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
