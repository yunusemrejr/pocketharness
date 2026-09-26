---
name: llm-fine-tuning
description: Build, debug and evaluate LLM training workflows using LoRA, QLoRA, PEFT, full fine-tuning, supervised instruction tuning or preference optimization, including memory planning, checkpoint recovery and model export.
---

# LLM fine-tuning

Produce a reproducible training artifact and evidence that it improves the intended task. First identify the base model and immutable revision, objective, deployment runtime, available GPU memory, dataset split manifest and success metric. Respect the user's chosen model and training method; use measured alternatives when constraints make that choice infeasible.

Read only the relevant references:

- [Method and resource planning](references/methods-and-memory.md): LoRA/QLoRA versus full tuning, continued pretraining, DPO, batch math and distributed choices.
- [SFT implementation](references/sft-recipes.md): a single-GPU trainer construction recipe, API compatibility checks, loss-mask inspection and smoke-to-training progression.
- [Evaluation, recovery and export](references/evaluate-resume-export.md): regression slices, forgetting, training checkpoints versus adapters, merge and deployment checks.

Use [LLM dataset preparation](../llm-dataset-preparation/SKILL.md) when collecting, converting, filtering, splitting or auditing examples. For a Colab runtime, use the available Google Colab skill for connection, authentication, persistence and runtime management; this skill handles training semantics.

Before a substantial run, inspect one actual collated batch and complete a small forward/backward/save/reload smoke test. Confirm supervised tokens survive truncation, padding has no loss, trainable parameters match the chosen method, gradients are finite, and a checkpoint can resume. A syntactically valid notebook is not a successful training run.

Treat library versions, quantization support and model templates as explicit contracts. Read the installed signatures and matching official documentation before adapting a recipe; never combine old `tokenizer`/`max_seq_length` arguments with newer `processing_class`/`max_length` examples blindly. Record exact package versions, model/tokenizer revisions, data hashes, seed, hardware, configuration and effective token budget.

Compare the untuned base with the tuned and exported models on the same frozen test prompts and decoding settings. Report quality, retention, cost and latency separately. The skill does not authorize a paid run, uploading private data, or publishing weights beyond the current task's authorization. Preparing a runnable notebook does not require performing those actions.
