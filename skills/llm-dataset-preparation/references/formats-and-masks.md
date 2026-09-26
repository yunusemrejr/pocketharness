# Formats, tokenization and masks

## Choose a representation that matches the objective

Text-only examples below describe common trainer families; check the installed trainer's schema before constructing the full dataset.

```json
{"text":"A complete document or text sequence for language modeling."}
{"messages":[{"role":"system","content":"Answer concisely."},{"role":"user","content":"What is 2+2?"},{"role":"assistant","content":"4."}]}
{"prompt":"Question: What is 2+2?\nAnswer:","completion":"4."}
{"prompt":"What is 2+2?","chosen":"4.","rejected":"5."}
```

TRL distinguishes language-modeling, prompt-completion and preference datasets, with conversational variants. Keep the entire intended prompt/context identical between preference alternatives. Do not concatenate a chat transcript into prose while also retaining a `messages` field that another adapter may render a second time. Select one model-facing schema and keep metadata in separate columns/artifacts. [TRL dataset formats](https://huggingface.co/docs/trl/dataset_formats)

Tool-use data needs the chosen model's actual chat-template contracts: supported roles, tool schemas, call IDs, argument serialization and matching result IDs. Preserve call/result order and missing/error results accurately. An assistant tool call may have no text content; this is valid in supported tool schemas but outside the bundled text-only audit helper. Multimodal examples similarly need media references, access checks, processor-specific masks and actual decode validation; do not stringify image/audio objects into accidental training targets.

## Render once and inspect actual tokens

Use the target tokenizer's chat template, not a generic `User:`/`Assistant:` concatenation. Training full conversations normally does not append an inference-only assistant generation prefix. When rendering to text first, avoid adding special tokens again during subsequent tokenization. Save tokenizer and template revisions with the data artifact; changing either can change the learning problem. [Transformers chat templates](https://huggingface.co/docs/transformers/chat_templating)

For representative examples, record:

1. Source turns and intended supervised spans.
2. Rendered text and `input_ids`, with special-token IDs identified.
3. `attention_mask`, labels and any assistant/completion mask after collation.
4. Decoded tokens whose label is not `-100` and their per-example count.

For causal LM training, trainers/models commonly shift labels internally; inspect the actual implementation instead of shifting manually twice. Do not blanket-mask every occurrence of the pad token ID if pad and EOS share an ID: real EOS targets would disappear. Distinguish padding by its mask/position. Conversely, attention masking alone does not necessarily exclude a token from the loss; labels need the correct ignore index.

Assistant-only and completion-only training are different objectives from all-token language modeling. Current TRL supports corresponding configuration and mask-bearing datasets; assistant-only masking requires a compatible chat template that exposes assistant generation spans. Verify the installed version and inspect its resulting batch. Pretokenized labels can change which trainer preprocessing is applied. A batch with zero supervised labels is unusable even if shapes are valid. [TRL SFT trainer](https://huggingface.co/docs/trl/sft_trainer)

Prefer a small explicit collator inspection over trusting argument names. A practical diagnostic is:

```python
batch = next(iter(trainer.get_train_dataloader()))
ids, labels = batch["input_ids"], batch["labels"]
assert ids.shape == labels.shape
assert (labels != -100).any(), "No supervised target tokens"
for i in range(min(3, len(ids))):
    target = labels[i][labels[i] != -100].detach().cpu().tolist()
    print(tokenizer.decode(target, skip_special_tokens=False))
```

Run this only on non-sensitive diagnostic samples; avoid printing private examples into shared Colab output. Some packed/padding-free trainers flatten examples, so interpret row counts according to the actual collator and inspect boundary metadata too.

## Length, packing and effective work

Measure token length after exact templating, separately for prompt, supervised completion and entire sequence. Report quantiles, maximum and truncation rate by task/language. Character counts are not token counts, especially for code, multilingual text or JSON.

Truncation can remove the answer, final EOS, tool result or crucial earlier constraint. Decide whether to reject, shorten context, split by semantic boundaries or permit truncation, and count outcomes. Do not truncate chosen/rejected answers asymmetrically into different preference tasks. For document chunks, keep source-group split membership and record overlap; repeated overlap inflates trained-token counts.

Packing improves utilization but can alter attention/boundary semantics. Verify whether samples can attend across boundaries, whether position IDs reset, and whether the selected attention kernel supports the requested packing implementation. EOS insertion alone is not isolation. Keep evaluation packing choices explicit and compare example-aligned metrics when that is the intended evaluation unit.

For device microbatch `b`, data-parallel workers `d`, accumulation `a` and optimizer updates `s`, nominal examples consumed are `b*d*a*s`; incomplete final batches, packing and iterable datasets can change that count. Useful training work is closer to the sum of supervised non-padding tokens. Compare throughput in those tokens per second, validation quality and actual wall time, rather than padded tokens or nominal batch size alone.
