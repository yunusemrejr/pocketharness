# Small Model Engineering: patterns and examples

## Make the task learnable
Separate failures of knowledge, instruction following, formatting, context handling and reasoning. Retrieval can supply missing facts but does not fix a broken decision policy. A typed tool contract and a short worked example may outperform a long generic system prompt. Keep one owner for state; external code should enforce ranges, schemas and authorization instead of hoping the model remembers them.

A useful pipeline is input normalization → model proposal → deterministic validation → one targeted repair or explicit fallback. Validation must check semantics where possible, not only JSON syntax. Bound retries and preserve the original request; do not hide repeated failure behind endless repair calls. Escalation to a larger model should obey configured provider/privacy/cost constraints, and should be measured as part of total cost.

## Data and distillation
Construct examples from actual error categories, including rejection/uncertainty and tool failures. Split by template, source or entity so near-duplicates do not inflate scores. Teacher outputs are candidate labels, not ground truth; verify consequential answers with code or authoritative data. Distillation can transfer teacher mistakes. Include short correct solutions and appropriate stopping, rather than rewarding verbosity as intelligence.

For fine-tuning, verify chat templates, loss masking and EOS. Compare to prompting-only and retrieval-only baselines before accepting added training complexity. Quantization can disproportionately damage a small model; measure task outcomes after conversion on the actual engine. A tiny validation set cannot establish broad capability.

## Resource-aware evaluation
Measure success per completed task, p95 latency, total generated tokens, retries and fallback frequency. Include long distractors, missing facts, contradictory requests and malformed tool returns. Keep enough context for the current decision; summarize historical evidence with provenance rather than silently deleting unresolved requirements. Use bounded context and incremental state, but never pretend a short model context contains information it discarded. A model that produces fewer tokens while doubling failures is not more efficient.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://huggingface.co/docs/transformers/
- https://huggingface.co/docs/trl/
- https://huggingface.co/docs/peft/

## Harness preprocessing and review assistance
Use the installed deterministic compactor first. The optional mini selector may choose exact complete source paragraphs; it cannot rewrite evidence or make quality decisions. Keep hashes, original retrieval, status, negation, qualifiers and dependencies. Reuse validated source-hash selections from a bounded session cache, clear them on branch/reset, and back off after unsupported or failed inference. Measure requests, cache hits, fallback rate, projected character savings and latency; character counts are not billed tokens. Do not enable an experimental model merely because its client exists.

For ML-assisted component checks, retain deterministic findings and source references. A learned reuse score cannot assess visual design or authorize repairs. Abstain across unsupported language/feature contracts, preserve divergence candidates, and test realistic changed tags, attributes, text, handlers and legitimate refactors. Evaluate any new weights on separated source families against the existing rule baseline before activation.
