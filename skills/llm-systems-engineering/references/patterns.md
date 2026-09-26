# LLM Systems Engineering: patterns and examples

## Architecture and memory
A rough KV cache byte estimate is `2 * layers * batch * cached_tokens * kv_heads * head_dim * bytes_per_element`; account for padding, paging, replication and engine metadata separately. GQA/MQA use KV heads, not query heads. Quantized weights do not imply quantized KV cache. Training additionally needs gradients, optimizer states and saved activations; checkpointing trades compute for memory.

Attention masks, positional encoding, sliding windows and packed sequences define semantics. Verify causal boundaries and position offsets across prefill/decode and prefix reuse. RoPE scaling is not evidence of retained long-context quality. Flash-style attention changes IO scheduling; approximate attention changes mathematical behavior and requires different evaluation.

## Kernels and precision
Compare fused kernels to a small high-precision reference over odd shapes, padding, masked rows and extreme logits. Stable softmax subtracts a row maximum, but fully masked rows still need an explicit contract. Quantization needs calibration provenance, supported group sizes/dtypes and checks on outliers and accumulated error. A lower nominal bit width can be slower if the target lacks efficient kernels or dequantization dominates.

## Fine-tuning and adaptation
Separate train/validation/test by source and contamination risk. Keep tokenizer/template, special tokens, label masking and EOS handling consistent. For LoRA specify targeted modules, rank, scaling and merge behavior; adapter savings do not remove base-model activation costs. Verify a tiny overfit test and gradient flow before scaling. Packed examples must not leak labels across boundaries. Record optimizer, precision, seed, data revision and checkpoint format.

Evaluate task correctness, tool calls, grounding and failure behavior—not just loss or fluent samples. Compare fixed prompt sets and token budgets, with cache state documented. Report time to first token, decode rate, total completion latency and memory separately. Validate checkpoint loading, distributed shard compatibility and rollback. Treat benchmark numbers from other hardware or engines as hypotheses, not predicted performance.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://pytorch.org/docs/stable/
- https://docs.vllm.ai/
- https://huggingface.co/docs/peft/
- https://arxiv.org/abs/2205.14135
