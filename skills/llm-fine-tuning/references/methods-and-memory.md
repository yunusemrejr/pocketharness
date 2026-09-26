# Method and resource planning

## Choose the objective before the optimizer

| Need | Candidate | Main diagnostic |
| --- | --- | --- |
| Stable output format, task behavior, instruction following | Supervised fine-tuning (SFT) | Held-out task success and format validity |
| Domain vocabulary and prose distribution | Continued causal pretraining, possibly followed by SFT | Domain loss plus retention of general ability |
| Prefer one plausible answer over another | DPO or another justified preference objective after a usable policy exists | Blind preference win rate and length/safety confounds |
| Frequently changing facts with attributable evidence | Retrieval/tool integration, optionally combined with tuning | Retrieval coverage and grounded answers |

Full tuning and adapters describe *which parameters change*, not the objective. Either can implement SFT or continued pretraining. DPO uses matched prompt/chosen/rejected examples and policy/reference likelihood differences; do not substitute ordinary answer-only rows. Account for reference computation and two responses per example. Precomputing reference log probabilities is conditional on a frozen reference and fixed data/tokenization. See [TRL DPO](https://huggingface.co/docs/trl/dpo_trainer).

## Parameter adaptation

For a matrix of shape `d_out × d_in`, ordinary LoRA adds `r(d_in+d_out)` parameters; the update is scaled by `alpha/r`. Count every targeted matrix, biases and extra saved modules when estimating optimizer memory. Rank is capacity, not a quality score. Compare a modest rank against one higher rank while holding data, token budget and evaluation constant; avoid changing rank, learning rate and dataset simultaneously.

Inspect `named_modules()` and the resulting trainable parameter names. Attention-only targets can reduce cost; `target_modules="all-linear"` broadens coverage for supported architectures. Architecture names differ, and MoE expert parameters may not be ordinary linear modules. `modules_to_save` makes selected non-adapter modules trainable and persistent: embeddings or a large output head can dominate memory. Vocabulary changes require resizing compatible embeddings before adapter wrapping and verifying that newly learned token weights survive export. Tied weights need explicit inspection. Advanced variants such as rank-stabilized LoRA, DoRA, adaptive ranks or initialization changes deserve an isolated comparison, not an automatic stack of options. See [PEFT LoRA configuration](https://huggingface.co/docs/peft/package_reference/lora).

QLoRA keeps a quantized base frozen while training adapters. For bitsandbytes 4-bit training, NF4, nested quantization and a supported compute dtype are relevant configuration choices. Prepare the quantized model before attaching adapters. Quantization does not make full base-weight training equivalent to QLoRA, nor guarantee a particular GPU fit. Confirm the actual backend and accelerator support. See [PEFT quantized training](https://huggingface.co/docs/peft/developer_guides/quantization).

## Calculate a budget, then measure it

An illustrative unsharded Adam estimate for `N` trainable weights with 2-byte weights, 2-byte gradients, 4-byte master weights and two 4-byte moments is `16N` bytes before activations or temporary buffers. Actual implementations can retain FP32 parameters or omit master copies, so calculate from observed dtypes and optimizer states rather than treating 16 bytes as a universal constant. Frozen 4-bit storage begins near `0.5N` bytes plus quantization metadata, nonquantized layers and adapter state; runtime peak can be much larger.

Ordinary data parallel effective example batch is `microbatch × accumulation × data_parallel_replicas`. Tensor/pipeline parallel ranks are not additional data replicas. Packing changes examples per sequence, and variable sequence lengths change useful tokens per update; record supervised tokens separately from padded/processed tokens. With 2 sequences/GPU, accumulation 8 and 2 data replicas, the nominal batch is 32 sequences. Exact optimizer-step counts also depend on sampler padding, drop-last and final accumulation handling: inspect the trainer's schedule.

Reduce sequence length only if targets remain intact. First measure peak memory on representative long rows through an optimizer step and evaluation. Gradient checkpointing trades recomputation for activation storage; smaller microbatches and accumulation trade throughput for fit. Attention backend support depends on dtype, architecture and hardware. Paged/8-bit optimizers change state management; they do not eliminate activations. Disable training KV cache where required. See [Transformers training performance](https://huggingface.co/docs/transformers/v4.37.2/en/perf_train_gpu_one).

For OOM, separate load, forward, backward, optimizer, evaluation and save peaks. Evaluation logits can exceed training memory: accumulate scalar/token-count statistics where possible, reduce eval batch, or move accumulated predictions to CPU. Record both allocated and reserved CUDA peaks; clearing the cache cannot free live tensors. Keep disk room for at least the next complete checkpoint before deleting older ones.

## Full tuning and distribution

Use unquantized trainable weights for ordinary full tuning; no PEFT wrapper or k-bit preparation. Start with a conservative learning-rate experiment relative to adapter tuning and monitor forgetting. BF16 requires accelerator support; FP16 typically uses loss scaling through the training framework. Do not launch full tuning merely because inference fits.

DDP replicates model and optimizer state. FSDP or DeepSpeed ZeRO can shard state; use one configured integration that matches the installed Transformers/Accelerate versions. FSDP1 and FSDP2 configs differ. Confirm wrapping policy, tied weights, mixed precision, activation checkpointing and adapter trainable/frozen compatibility with that backend. CPU/NVMe offload trades device memory for host capacity and transfer time. Do not use inference `device_map="auto"` as a distributed training strategy. Save a resumable distributed checkpoint and test a separately consolidated inference export; changing world size may need explicit support. See [Accelerate FSDP/DeepSpeed comparison](https://huggingface.co/docs/accelerate/concept_guides/fsdp_and_deepspeed) and [Transformers FSDP](https://huggingface.co/docs/transformers/main/fsdp).
