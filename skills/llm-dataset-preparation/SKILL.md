---
name: llm-dataset-preparation
description: Prepare and audit datasets for LLM supervised fine-tuning, continued pretraining and preference learning, including conversation schemas, leakage-resistant splits, chat templates, loss masks and token budgets.
---

# LLM dataset preparation

Produce a versioned training artifact and a data-quality report, not just a JSONL conversion. Establish the intended behavior, source rights, task/language mix, model tokenizer/template and evaluation unit. Preserve raw inputs separately from derived examples; record every exclusion and transformation with source IDs.

Choose the objective before the schema: document text for continued pretraining; high-quality demonstrations for SFT; matched prompt/chosen/rejected pairs for preference training. Do not turn arbitrary transcripts or model-generated answers into verified targets without review.

Read only the reference relevant to the current stage:

- [Source preparation and split design](references/preparation-and-splits.md): extraction, cleaning, provenance, licenses, entity/time splits, duplicates, synthetic data and mixtures.
- [Schemas, tokenization and label masks](references/formats-and-masks.md): text/chat/prompt-completion/preferences, tool traces, EOS/padding, loss masks, packing and length budgets.
- [Auditing and reproducible delivery](references/audit-and-delivery.md): local helper, token-level checks, streaming, manifests and acceptance evidence.

Use `scripts/audit_jsonl.py --train train.jsonl --validation validation.jsonl --test test.jsonl` for a bounded, dependency-free structural and exact-overlap audit. It reads explicit local files, reports counts and line references, and never rewrites data. It does not detect paraphrase leakage, private information, tokenizer errors or label correctness. Its text-only schema deliberately rejects multimodal/tool-call-only records rather than guessing how to convert them.

Before a large run, inspect the trainer's actual post-collation batch: decode inputs and supervised labels separately, count non-ignored labels and verify EOS treatment. Preserve the test split while choosing filters/hyperparameters on train/validation. For training and adapter/export decisions use [LLM fine-tuning](../llm-fine-tuning/SKILL.md); for notebook execution use [Google Colab training](../google-colab-training/SKILL.md).

Deliver schema and source revisions, split/group policy, quality/overlap counts, tokenizer/template fingerprints, length and supervised-token statistics, excluded-row reasons and an example batch inspection. A structural pass is only one part of readiness.

> PocketHarness note: this skill's bundled code assets were removed (the harness ships only native C/C++ and bash). Write the equivalent in the project's own language when needed, and prefer `pocket kit` (wav, audio, spring, svg, shot, web) where it covers the job.
