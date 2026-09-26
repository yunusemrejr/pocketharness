---
name: ml-engineering
description: Engineer ML data pipelines, leakage prevention, reproducible training, evaluation, serving and MLOps. Use for model development and production ML reliability.
---


# ML Engineering

## Principle: it's a data system first, a model second

Most production ML bugs are data bugs. The model is a compiled artifact of the pipeline state; if the pipeline is wrong, a better model ships the same bug at higher speed.

Before experiments, state the target, unit of prediction, prediction time, simple baseline and held-out success metric. Inspect the real schema and label availability. Split by entity/time as appropriate before fitting imputers, scalers, feature selection or tuning; fit learned preprocessing on training only. Record executed commands and data versions, not hypothetical scores. Report uncertainty and subgroup errors alongside aggregate metrics. These checks apply to small experiments too; do not add production infrastructure to a notebook without need.

## Data

- **Point-in-time correctness (leakage) is the #1 failure.** Features must use only data available *at prediction time*. Backfilling a "user_tenure_months" computed from a view that includes future rows poisons training and silently flatters metrics. Rule: every feature has its *event time* and its *availability time*, and the join key is (entity, availability_time ≤ decision_time).
- **Feature pipeline = idempotent, versioned, replayable.** Same input window → same features, every run. Version the pipeline code alongside the model (store the pipeline version in the model registry entry). Backfills must produce byte-identical features for overlapping windows — test this.
- **Train/serve parity:** the feature *computation* must be the same code (or a verified match) in training and serving — two implementations that "should agree" drift within a quarter. Where they must differ (batch vs streaming), add a consistency check: sample N online requests, recompute features offline, diff distributions (population displacement/PSI per feature, or KS test).
- Data contracts: schema (types, nullability, domains), volume floors/ceilers, freshness SLA on every input. Contract violation → pause, alert, don't "best effort" through.
- Label hygiene: label latency (conversion takes 7 days?) shifts distribution between train and serve; sample bias (labels exist only for the clicked/impressed population) is a silent survivorship bias — model the *decision* you want, not the label you have, or correct the sampling explicitly.

## Training & experiments

- **Reproducibility checklist:** seed (pytorch/np/random + dataloader worker seeds), library versions (lockfile), dataset snapshot ID (not "the latest partition"), config file (checked in, hash in the run record), environment (container image). A run you can't reproduce is a rumor.
- Config management: one source (hydra/omegaconf or plain JSON/YAML), overrides only on the command line, run records (MLflow/W&B/Vertex ML) keyed by config hash + data snapshot + code commit.
- Validation split ≠ test split: report test-set metrics **once** per experiment, keep test untouched; iterate on val. Repeated peeking at test converts it into val.
- Checkpointing: write every N steps, keep best-by-val, resumable from the exact step (optimizer state included — skipping optimizer state changes convergence).
- Scale: DDP for multi-GPU (set epoch-shuffled, grad-accumulate to match effective batch); spot instances + checkpoint-resume for cost; log per-step: loss, val metrics, LR, grad norm — the loss curve is the monitoring dashboard.

## Evaluation (the discipline that separates ML from demos)

- **Offline metrics lead, online metrics decide.** A/B (or interleaving for ranking): 1 primary, ≤ 3 secondary, pre-declared guardrails (latency, cost, churn). Significance: power analysis or sequential testing; minimum detectable effect vs. your traffic. Never read the dashboard and declare victory on the first green day.
- Metric choice: per-task — classification: F1/PR-AUC with class imbalance (accuracy is a lie at 1% positives); ranking: nDCG; generation: task-specific judge + human sample; regression: MAE/quantile for the tail you actually serve.
- **Calibration** matters when the probability is a decision (risk, bid): report reliability curves / ECE; fix with Platt/isotonic or temperature scaling.

## Serving

- Format: ONNX (universal), TensorRT (NVIDIA GPU), TFLite (mobile), TorchServe/Trt-LLM for LLM inference. **Quantize in the order you'd stop:** fp32 → fp16 (GPU) → int8 (with calibration data from your domain, not random) → int4 (LLM weight-only is common). Re-run your eval after each step; a 1% offline hit can hide 5% on the tail.
- Latency budget: set the p95 target from the product experience, then: batching (dynamic/cushion batch), precision, model (a smaller model at 3× speed beats a bigger model at the same quality on serving cost), precompute & cache embeddings.
- Cold starts: warm up the first N requests (JIT/autotune), load-balancer min-replica on GPU boxes.
- Cost: track $/1k predictions; a quality gain that doubles cost needs a business sign-off with numbers.

## MLOps / deployment

- **Model registry** (MLflow, SageMaker, Vertex): model + pipeline version + feature defs + eval report + approval state. Deploy by registry version, never "the notebook's `model.pkl`."
- **CI for models:** a pipeline job runs the frozen eval set on every model candidate; candidate fails the eval gate → no deploy. Same as code reviews, but for models.
- Deploy patterns: shadow (log only, measure against live) → canary (1% → 10% → 100% on automated + human metrics) → full. **Always keep the last-known-good model one flag flip from restorable.**
- **Drift monitoring:** input drift (PSI/KL on features vs. training), prediction drift (distribution shift of scores), label drift (positive rate, when labels are delayed). Alert on drift; don't just chart it.
- Feedback loops: log every prediction + inputs + (later) label — the *inference log* is your training data's future and your debugging's past. Sample-based logging is fine under full volume; full sample at low volume.

## LLMOps (specifics)

- Fine-tune only when: style/format must be consistent at scale, or the base model's domain vocabulary genuinely fails, or latency/cost demands a smaller model. **Prompt + retrieval + strong eval first.**
- Dataset > model: 200 clean, diverse, verified examples beat 10k sloppy ones. Version the dataset; dedupe; hold out an untouched eval.
- Eval harness: golden set (curated, versioned, re-usable), per-category breakdown (a 5% drop hidden by a 95% category), LLM-as-judge calibrated against human labels (report agreement rate), regression suite in CI (same as code: red = blocked).
- Cost control: semantic cache (near-duplicate questions), model ladder (small-first, escalate on confidence/length heuristics), streaming to the user, batch API for non-urgent.
- Safety & privacy: PII never in training data without a pipeline; prompt-injection boundaries (see `ai-engineering`); log what the model saw for the audit, not the whole conversation verbatim.

## Debugging an ML system (in order)

1. Data pipeline (leakage, freshness, contracts) · 2. train/serve feature diff (consistency check) · 3. labels (delay, bias, definition drift) · 4. eval validity (is the offline metric even correlated with the online one?) · 5. model (did it actually train — loss curve, LR schedule, batch size) · 6. serving (quantization delta, batching, timeout fallbacks).

## Detailed coverage

ML systems engineering — data pipelines & leakage, train/serve consistency, evaluation discipline (offline vs online), reproducibility, serving (ONNX, quantization, batching), MLOps deployment (CI eval gates, canary, drift monitoring), and LLMOps specifics. Use when building, shipping, or debugging ML systems end-to-end (not one-off notebooks).
