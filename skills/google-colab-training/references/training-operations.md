# Training operations

## Environment and reproducibility

Record Python, GPU name/VRAM, driver, PyTorch and CUDA build, BF16 capability, dependency versions, code revision, model/tokenizer revisions, dataset hashes, split IDs and seeds. A frozen package list alone is not a reproducible GPU environment: wheel platform, driver compatibility and optional kernels matter. Use a small explicit lock/constraints file for the selected recipe, preserve the platform's working torch stack unless replacement is necessary, then run `python -m pip check`. Restart the kernel after replacing already imported binary packages; rerunning an import cell does not unload them.

Prefer `%pip` in notebooks or `sys.executable -m pip` from Python. Pin the tested Transformers/TRL/PEFT/Accelerate/bitsandbytes combination rather than independently upgrading everything. Do not build FlashAttention as the first diagnostic step on an unknown accelerator. First verify a supported baseline attention implementation, forward/backward pass and adapter gradient flow.

## Dataset staging

Validate schema, license/access, train/eval separation, role order, token lengths, target masks and finite training loss before transferring the complete dataset. A tiny stratified sample should include the longest examples and tool/multiturn cases, not only easy short rows.

Stage immutable shards to local scratch, with a manifest containing bytes, checksum and source revision. Download to a `.partial` path, verify completion/checksum, then publish the local filename. Bound disk use for downloads, extracted data, tokenizer caches, optimizer checkpoints and final exports simultaneously. Streaming lowers disk pressure but changes shuffle/resume semantics: record shard, row and sampler state or document replay. Never treat a streaming iterator's position as a complete reproducible checkpoint.

Batch local reads and checkpoint uploads instead of training directly against thousands of Drive files. Copy rather than move the only durable copy. Colab warns that interrupted Drive moves can lose data; resources and runtime lifetimes are variable. Plan for interruption rather than depending on fixed hours or bypassing idle restrictions. [Colab resource and storage behavior](https://research.google.com/colaboratory/faq.html)

## Resume contract

A usable training checkpoint is more than exported adapter weights. Record model/adapter weights, optimizer, scheduler, RNG and trainer/sampler state, step, model/data revisions, precision and effective batch configuration. Confirm the trainer actually saved required state. A portable inference export and a resumable checkpoint are separate artifacts. `Trainer.train(resume_from_checkpoint=...)` restores supported checkpoint state; `save_only_model` sacrifices normal optimizer/scheduler resume. Test an interrupted short run against an uninterrupted reference within expected numerical variation before a long run. [Trainer behavior](https://huggingface.co/docs/transformers/main/en/trainer)

Write checkpoints locally, close files, produce a checksum manifest, copy/upload into a new durable step-specific prefix, verify object sizes/hashes, and publish a small completion marker last. Resume only completed checkpoints. Do not assume a cloud object prefix or mounted Drive rename is transactional. Keep the previous verified checkpoint until the new one passes verification; bound retention and account for upload time. In distributed jobs, follow the trainer's rank coordination and sharded-state protocol rather than uploading an incomplete rank-zero directory.

At resume, compare the manifest before accepting state. Changed base revision, tokenizer vocabulary, adapter targets, optimizer or data order can invalidate it. If only adapter weights remain, label the run a warm start, initialize optimizer deliberately and record the lost training state. Avoid claiming exact continuation.

## Memory and throughput diagnosis

Distinguish GPU OOM, host-RAM exhaustion and filesystem exhaustion; their fixes differ. The resource monitor and `nvidia-smi` report device use; PyTorch's allocated versus reserved statistics explain its allocator. `empty_cache()` releases unused cached blocks, not live tensors. Remove retained computation graphs and references before using it. Do not lower numerical precision indiscriminately to fix a memory leak. [PyTorch CUDA memory management](https://docs.pytorch.org/docs/stable/notes/cuda.html)

For GPU OOM, measure peak use on a representative long batch. Reduce microbatch and/or sequence length, enable supported activation checkpointing, disable training-time KV cache when required by the model, then consider quantized adapters or smaller models. Increasing gradient accumulation can restore effective batch but does not shrink a single sequence's activation peak. LoRA trainable parameter count does not include the frozen base or activations. Full tuning may need optimizer-state sharding/offload; account for CPU RAM and slower transfers rather than assuming one consumer GPU can fit it.

If system RAM fails, reduce simultaneous dataset materialization, worker count and decoded caches; use memory-mapped/sharded data. If disk fails, inspect run/cache sizes and remove only confirmed redundant artifacts after durable verification. A CUDA illegal-memory-access failure may require a kernel restart; treat the checkpoint as suspect until validated.

## Cost, run plan and acceptance

Measure tokens/sec and peak memory after warmup. Estimate training time as planned processed tokens divided by observed tokens/sec, then add evaluation, checkpointing, startup and transfer time. Count repeated epochs and padding overhead consistently. Measure useful target tokens separately from total processed tokens when comparing masking strategies. Small-sample throughput is an estimate, especially when sequence lengths vary.

Set a bounded smoke run, then a maximum step/time budget for full training. Refresh current account pricing/compute-unit behavior and report an estimate with assumptions; never promise a specific GPU, free capacity or uninterrupted duration. Persist the useful artifacts before ending the session. Confirm the intended session stopped rather than assuming closing a tab released compute.

Accept a run only after checking finite loss/gradients, expected trainable modules, held-out metrics against the unchanged base, representative generations, clean-process export loading and checkpoint restoration. A completed progress bar is not model-quality evidence. Report failure cases and limitations, especially if GPU training was prepared but could not be executed.
