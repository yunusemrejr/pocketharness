---
name: small-model-engineering
description: "Adapt small language models through focused data, distillation, constrained workflows and task evaluation."
---

# Small Model Engineering

Use for SLM training and integrations where memory, latency or cost limits dominate. Small models need clear contracts and measurable tasks, not inflated promises.

## Working method

- Define a narrow target task, failure cost and stronger baseline.
- Reduce ambiguity with typed inputs and bounded workflows.
- Choose retrieval, fine-tuning or distillation based on observed errors.
- Evaluate on unseen realistic cases and document fallback behavior.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.

## Harness integration
Use deterministic compaction first, then validated extractive selection with original retrieval. Cache exact-source selections, bound CPU and retries, and retain protected evidence. See `references/patterns.md` for cache, telemetry and ML component-review contracts; client presence is not model eligibility.
