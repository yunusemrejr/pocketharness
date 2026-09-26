# Auditing and delivery

## Local structural helper

`python scripts/audit_jsonl.py --train train.jsonl --validation validation.jsonl --test test.jsonl` inspects explicit UTF-8 JSONL files. Each row must use exactly one supported text-only family: `text`, `messages`, `prompt/completion`, or `prompt/chosen/rejected`. Metadata is allowed. `group_id` and `id`, when supplied, must be nonempty strings. The same source entity/document/conversation should share `group_id` across its derived rows.

The helper checks malformed JSON, duplicate JSON keys, non-finite constants, blank targets, conversation role/content shape, missing user/assistant turns, identical preferences, duplicate IDs and cross-split exact example/prompt/group overlap. Reports contain counts and bounded line references, never example text. Exit 1 means structural/leakage errors; exit 2 means unreadable input or a resource limit. Within-split duplicates are counted for review, not automatically removed. Limits are 1 MiB per physical line and 200,000 rows overall; larger corpora need sharded manifests and an external sort/SQLite/Arrow implementation of the same checks.

It intentionally does not certify: legal permission, PII removal, source authenticity, near-duplicate absence, correctness, balanced task coverage, assistant masks, multimodal validity or tokenizer alignment. Report those separately. Do not make an arbitrary corpus pass by silently deleting problematic examples; retain exclusion counts and reasons.

## Large datasets and resume behavior

Use bounded staged artifacts on a Colab runtime's local disk for random reads; copy versioned shards/checkpoints to durable storage. Avoid millions of tiny files and repeated remote tokenization. Store preprocessed Arrow/Parquet or explicit JSONL shards with checksums and a manifest. HF iterable streaming reduces materialization needs, but buffered shuffle is approximate, and order, worker/shard configuration and epoch handling matter. A resumed stream is not necessarily the exact next sample unless its state and sampler position are restored and verified. Define steps/token budget for unknown-length streams and detect unintentional repetition across workers. [Datasets streaming](https://huggingface.co/docs/datasets/stream)

Keep preprocessing and training randomness distinct. Persist package versions, dataset revision, transform code hash, tokenizer revision, template digest, split membership and seed. A cache fingerprint is useful but does not replace a source manifest when external state or mutable model revisions affect preprocessing.

## Acceptance evidence

Before full training, deliver a report with original/accepted/excluded counts; duplicate and overlap counts; group and time ranges; task/language/source distributions; token-length/truncation statistics; supervised-token counts; and a reviewed sample from each important slice. Decode at least one long example, one multi-turn example and an error/tool path if included. Confirm the loss ignores exactly the intended tokens.

Freeze an evaluation snapshot before iterative tuning. Match deployed response formatting, decoding controls and task metrics. Keep benchmark reference answers out of both demonstrations and synthetic-generation prompts. Include abstention, ambiguous questions, malformed inputs and regressions relevant to the product. Track source/model/template revisions in evaluation records so apparent progress is not a moving test.

A handoff manifest can contain `dataset_id`, `source_revisions`, `schema_version`, `transform_commit`, `tokenizer_revision`, `chat_template_sha256`, `split_policy`, `split_files_sha256`, `counts`, `token_statistics`, `quality_report`, and `known_limitations`. Use real hashes/counts; mark unavailable evidence rather than inventing it. Save the model-facing artifact separately from audit metadata, access credentials and human notes. Publishing a dataset or uploading private examples is a separate operation from preparing it locally.

Source/API guidance checked against official documentation on 2026-09-09. Recheck installed package signatures before adapting version-dependent preprocessing code.
