# Single-GPU SFT recipe and verification

This is a **version-adaptable recipe, not a GPU-tested environment lock**. It uses the TRL API family with `SFTConfig.max_length` and `SFTTrainer.processing_class`, documented in [TRL 0.21 SFT](https://huggingface.co/docs/trl/v0.21.0/en/sft_trainer). Resolve a mutually compatible package set for the selected model and GPU, inspect installed signatures, then freeze it. Latest package versions are not assumed compatible. Modern Transformers may use `dtype`; the model-loading spelling below uses the older `torch_dtype` contract and must be checked when updating.

Inputs: local JSONL files with audited `prompt` and `completion` string columns, nonempty disjoint splits, `MODEL_ID`, immutable `MODEL_REVISION`, `MODE` (`lora`, `qlora`, or `full`), and an unused local `RUN_DIR`. Chat datasets should instead retain `messages` and use the matching template as described below. Secrets belong in runtime secret storage, not these files. This construction cell does not start training or publish artifacts.

```python
import inspect, json, os
from importlib.metadata import version
from pathlib import Path
import torch
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig, set_seed
from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training
from trl import SFTConfig, SFTTrainer

required = {"max_length", "completion_only_loss", "eval_strategy", "save_steps"}
assert required <= set(inspect.signature(SFTConfig).parameters), "Adapt to installed TRL"
assert "processing_class" in inspect.signature(SFTTrainer).parameters
assert torch.cuda.is_available(), "This recipe requires a CUDA runtime"
assert int(os.environ.get("WORLD_SIZE", "1")) == 1, "Use a separate distributed recipe"
mode = os.environ["MODE"]
assert mode in {"lora", "qlora", "full"}
model_id, revision = os.environ["MODEL_ID"], os.environ["MODEL_REVISION"]
assert len(revision) == 40 and all(c in "0123456789abcdef" for c in revision.lower())
out = Path(os.environ["RUN_DIR"])
out.mkdir(parents=True, exist_ok=False)
seed = 1729
set_seed(seed)
bf16 = torch.cuda.is_bf16_supported()
dtype = torch.bfloat16 if bf16 else torch.float16
packages = ["torch", "transformers", "trl", "peft", "accelerate", "datasets"]
if mode == "qlora":
    packages.append("bitsandbytes")
manifest = {"versions": {p: version(p) for p in packages}, "model": model_id,
            "revision": revision, "seed": seed, "mode": mode,
            "gpu": torch.cuda.get_device_name(0), "cuda": torch.version.cuda}
(out / "environment.json").write_text(json.dumps(manifest, indent=2))
tokenizer = AutoTokenizer.from_pretrained(model_id, revision=revision)
assert tokenizer.eos_token_id is not None, "Choose and audit the model's end token"
if tokenizer.pad_token_id is None:
    tokenizer.pad_token = tokenizer.eos_token
tokenizer.padding_side = "right"
data = load_dataset("json", data_files={"train": "train.jsonl", "validation": "validation.jsonl"})
for split in data.values():
    assert len(split) and {"prompt", "completion"} <= set(split.column_names)

# FP16 AMP full tuning keeps FP32 trainable weights for gradient unscaling.
weight_dtype = torch.float32 if mode == "full" and not bf16 else dtype
load_args = {"revision": revision, "torch_dtype": weight_dtype}
if mode == "qlora":
    load_args.update(device_map={"": 0}, quantization_config=BitsAndBytesConfig(
        load_in_4bit=True, bnb_4bit_quant_type="nf4",
        bnb_4bit_use_double_quant=True, bnb_4bit_compute_dtype=dtype))
model = AutoModelForCausalLM.from_pretrained(model_id, **load_args)
model.config.use_cache = False
if mode == "qlora":
    model = prepare_model_for_kbit_training(model, use_gradient_checkpointing=True)
if mode != "full":
    model = get_peft_model(model, LoraConfig(
        task_type="CAUSAL_LM", r=16, lora_alpha=32, lora_dropout=0.05,
        target_modules="all-linear", bias="none"))
    model.print_trainable_parameters()
trainable = sum(p.numel() for p in model.parameters() if p.requires_grad)
assert trainable > 0

# Three optimizer steps are a mechanics smoke test, not a quality experiment.
args = SFTConfig(
    output_dir=str(out), max_length=512, packing=False,
    completion_only_loss=True, per_device_train_batch_size=1,
    per_device_eval_batch_size=1, gradient_accumulation_steps=2,
    max_steps=3, learning_rate=1e-5 if mode == "full" else 1e-4,
    optim="adamw_torch", gradient_checkpointing=True,
    bf16=bf16, fp16=not bf16, logging_steps=1,
    eval_strategy="steps", eval_steps=3, save_strategy="steps", save_steps=3,
    save_total_limit=2, report_to="none", push_to_hub=False,
    seed=seed, data_seed=seed,
)
trainer = SFTTrainer(model=model, args=args, processing_class=tokenizer,
    train_dataset=data["train"], eval_dataset=data["validation"])
batch = next(iter(trainer.get_train_dataloader()))
labels = batch["labels"]
assert (labels != -100).sum().item() > 0, "All supervised tokens were masked"
assert ((labels != -100).sum(dim=1) > 0).all(), "A row has no training target"
assert (labels[:, 1:] != -100).any(), "No target survives causal shift"
if "attention_mask" in batch:
    assert (labels[batch["attention_mask"] == 0] == -100).all()
print("Supervised tokens:", (labels != -100).sum().item())
# Inspect decoded supervised spans locally without persisting private examples.
# Audit prompt exclusion, true EOS labels and truncation before running train().
```

After inspecting the batch, run the following in the authorized compute environment. Save the dataset hashes, split manifest and complete training arguments beside `environment.json`; seed alone is not a reproducibility record.

```python
torch.cuda.reset_peak_memory_stats()
trainer.train()
trainer.save_model(str(out / "export"))
tokenizer.save_pretrained(str(out / "export"))
print("Peak allocated bytes:", torch.cuda.max_memory_allocated())
```

The recipe deliberately avoids silently accepting unsupported arguments. It also avoids wrapping PEFT twice: because `model` is already adapted, no `peft_config` is passed to the trainer. To continue an existing adapter, load it trainable onto the matching base rather than initializing a fresh adapter. Full tuning should inspect that intended base parameters, not only a head, receive gradients.

## Chat, padding and packing

Use the training template belonging to the model revision. Render training conversations without an extra assistant-generation prefix. If a rendered string already includes special tokens, avoid adding them a second time in a later tokenizer call. Tokenization of separately encoded prompt and response may differ at the boundary; use the trainer's documented prompt/completion path and inspect labels. See [Transformers chat templates](https://huggingface.co/docs/transformers/chat_templating).

For conversational SFT, `assistant_only_loss=True` requires a template that supplies assistant generation masks. Some TRL versions patch selected templates, but verify the actual rendered masks rather than assuming family support. Completion-only and assistant-only are different masks. Ensure tool messages and intermediate user turns receive the intended loss. Padding by token ID alone is wrong when EOS is also PAD: real end-of-turn tokens must remain supervised while padding positions are masked. The basic assertions above cannot prove correct span selection; inspect at least a multi-turn, long and empty-response edge case. See [current TRL SFT semantics](https://huggingface.co/docs/trl/sft_trainer).

Keep packing off until unpacked training is correct. With packing, inspect document boundaries, EOS, attention isolation and position resets for the specific attention backend. Packing is not proof of cross-example isolation. Compare loss and outputs on packed/unpacked fixtures before crediting a throughput gain.

## Progress from smoke to useful training

Overfit a tiny authorized subset to expose label or optimizer bugs, then discard that checkpoint as an evaluation candidate. For the real experiment, increase sequence length and run length from measured dataset lengths and budget; retain a bounded validation cadence and resumable saves. Track loss, finite gradient norms, trainable-weight changes, useful tokens/sec, peak VRAM and validation task metrics. Zero loss can indicate all-masked labels; falling training loss with worsening task success can indicate overfitting or template mismatch. NaN/Inf calls for checking data, precision and learning rate before launching again.

A fixed seed does not promise identical results across hardware, kernels or releases. Record deterministic settings when needed and measure their speed cost. See [PyTorch reproducibility](https://docs.pytorch.org/docs/2.9/notes/randomness.html).
