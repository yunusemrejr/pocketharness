# Evaluation, recovery and export

## Decide whether tuning worked

Freeze a base-model baseline before training. Keep validation for checkpoint/hyperparameter selection and a separate test set for final reporting. Split by provenance groups before deriving variants: duplicate prompts, document fragments and synthetic siblings can inflate apparent quality. Include production task slices (language, domain, difficulty, long context, output format), a general-capability retention set and applicable safety/privacy checks. Dataset engineering lives in [dataset preparation](../../llm-dataset-preparation/SKILL.md).

Use executable task metrics when available: schema validity, exact answer with approved normalization, unit-test pass rate or retrieval-grounded correctness. For open responses use blinded pairwise review with order randomized; report disagreements and failure examples. Lower validation loss is not necessarily better generation. Count tokens consistently when aggregating perplexity, and only compare models with compatible tokenization/objectives; masked instruction loss is not whole-corpus perplexity.

Hold prompt template, system message, sampling, stop tokens, context budget and runtime constant for base-versus-adapter comparisons. Repeat across seeds if differences are small enough to affect a decision. Report sample counts and uncertainty, not just a percentage. Inspect verbosity and refusal-rate changes so a judge does not reward longer answers automatically. Do not keep tuning against the final test set.

Track wall time, accelerator type, completed optimizer steps, supervised tokens, memory peaks and cost assumptions. Exclude setup/download time only if separately reported. An adapter that improves the target but breaks a required language or schema is a regression, even with a better aggregate score.

## Training checkpoint versus model artifact

A resumable training checkpoint includes optimizer/scheduler state, step and RNG state, plus the model or adapter state the trainer expects. Preserve sampler/data order and the exact preprocessing manifest; streaming datasets require a documented replay/position strategy. Test recovery by saving after a few updates, recreating the environment and verifying the next step, learning rate and parameters advance from the saved state. Use the trainer's `resume_from_checkpoint=<verified checkpoint directory>` path with a compatible configuration. Changing total steps can change the scheduler; document intentional changes. See [Transformers Trainer](https://huggingface.co/docs/transformers/trainer).

An adapter export is usually insufficient to resume the identical optimizer trajectory. It requires the original base model; keep its revision, tokenizer/template, adapter configuration and any trained extra modules. Loading an adapter with `is_trainable=True` allows further training but does not reconstruct a lost optimizer. `merge_and_unload()` removes the adapter structure from the returned model and is an inference-export decision. See [PEFT checkpoint format](https://huggingface.co/docs/peft/developer_guides/checkpoint).

## Export verification

Prefer an untouched adapter export as the recoverable source artifact. Reload the precise base and tokenizer in a fresh process, load the adapter, enter evaluation mode, and compare fixed-prompt outputs/logits within dtype-appropriate tolerances. Check every added token, end marker, custom head and tied embedding. Full tuning exports full weights and tokenizer; distributed checkpoints may need consolidation before a standard loader can read them.

If deployment requires a merged model, reload a compatible floating-point base with sufficient host/device memory, load the adapter, merge using the supported API and save separately. Quantized merges depend on method/backend and may be unsupported or numerically different; do not assume every quantizer accepts merge/unmerge. Requantizing a merged model is a new artifact requiring evaluation. See [PEFT quantization constraints](https://huggingface.co/docs/peft/developer_guides/quantization).

Before deployment, verify target engine support for architecture, tokenizer, chat template, context length/RoPE settings, quantization format and adapter targets/rank. A runtime that loads base weights can still ignore adapters or use an incorrect template. Compare the exported runtime against the training-side baseline on a small fixed fixture, then the held-out evaluation. Measure cold load, first-token latency, steady token rate and peak memory separately.

The release manifest should identify artifact hashes, base/model/tokenizer revisions, license and data-use constraints, package lock, training objective, data/split hashes, trainable modules, optimization settings, evaluation results and known failures. Save locally unless the task authorizes publication; a Hub push or cloud upload can expose training examples in metadata, notebook outputs or checkpoints.
