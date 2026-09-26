# Preparation and split design

## Define the unit that may generalize

Start with the deployed query and the evidence needed to answer it. A support assistant may need coverage by product, language, troubleshooting stage and escalation condition; a code model may need repository-level separation; document extraction may require source-document separation. Write success and failure examples before synthesizing volume. Fine-tuning is not a substitute for supplying changing facts through retrieval.

Keep `source_id`, source revision/hash, original document or conversation identity, collection time, language, task category, transformation version and licensing/permission basis outside the text presented to the model. Keep annotations and human adjudication history separate from target text. Remove credentials and identify personal/sensitive data according to the actual dataset's authorized use. Do not assume publicly downloadable data permits redistribution or model training. A dataset card should explain intended use, composition, collection, annotation, limitations and license rather than only listing row counts. [Hugging Face dataset cards](https://huggingface.co/docs/hub/datasets-cards)

For OCR/PDF/HTML extraction, sample actual output around tables, formulas, headings and page boundaries. Repeated navigation bars and footers can dominate token frequencies. Preserve meaningful whitespace in code and poetry; do not lowercase identifiers, normalize away mathematical distinctions, strip accents or delete minority scripts under a generic cleanliness rule. Unicode normalization is a task decision, not automatic correction. Store raw bytes/hashes before destructive cleaning. Separate malformed encoding from legitimate multilingual content.

## Split by source relationships

Create groups from documents, repositories, conversations, users or other dependence units before chunking, augmentation or synthetic expansion. Put all derived variants of a group in the same partition. Build exact/near-duplicate candidate components across source groups before assigning splits; otherwise copied material can cross group boundaries. Normalization for duplicate detection must be stored separately from the canonical training text. Semantic similarity finds review candidates, not automatic equivalence.

Use time cutoffs when evaluating future behavior. Ensure training examples and their answers/evidence existed before the cutoff. Do not use future annotations to construct training features. For entity generalization, hold out entities; for in-domain continuation, stratified groups may be appropriate. A random row split is justified only when rows are plausibly independent for the target claim.

For stable non-temporal assignment, one simple policy is `u = int.from_bytes(sha256((seed + '\0' + group_id).encode()).digest()[:8], 'big') / 2**64`, with thresholds recorded in the manifest. Adding groups then leaves previous assignments unchanged. This does not guarantee exact row proportions, balanced languages or balanced group sizes; check realized distributions and use deterministic group-stratified allocation where needed. Never randomly split chunks from the same source separately.

Audit source-group overlap, exact example overlap and prompt overlap across train/validation/test. Same prompt with different answers can still leak evaluation task identity. Similarity review should include paraphrases, translated copies, template-generated variants and benchmark text. Maintain validation for iteration and a separately frozen test set. If the test set influences training choices, report it as development data and reserve another untouched evaluation.

HF `Dataset.train_test_split` can assign independent rows, while `map`/`filter` transform data and return derived datasets. They do not infer your group/time leakage policy. Explicitly persist the assigned split and source group before mapping to chunks. For batched expansion, check output-column lengths, remove incompatible original columns, and retain provenance for every new row. Save a stable processed snapshot rather than relying on a notebook cache as the dataset's identity. [Datasets processing](https://huggingface.co/docs/datasets/process)

## Labels, synthetic data and mixtures

Review answers for correctness, relevance, completeness, unsupported certainty, formatting and appropriate uncertainty. Boilerplate, accidental analysis traces, stale tool results and copied system instructions can become learned behavior. For agent trajectories preserve the distinction between trusted instructions and untrusted observations; include recovery from tool errors without fabricating successful execution. Truncate or redact secrets without creating impossible examples.

Synthetic data needs teacher/model/template/seed provenance and task-specific verification. Split source prompts before generation. Keep failed attempts and rejection reasons in audit data rather than silently training on every generated answer. Check teacher licensing and training permissions. An agreement between the generator and its own judge is not independent ground truth. Use executable validators where outputs have verifiable contracts and human adjudication where they do not.

For preference pairs, the prompt/context must match; isolate which property makes one answer preferable. Reject identical responses, label inversions and pairs decided only by verbosity unless verbosity is the intended objective. Record ties separately. Do not mix incompatible rating rubrics without calibration.

Design mixtures using both examples and effective supervised tokens. A small number of long documents can dominate optimization. Report per-task/language token share and sampling/repetition caps. Oversampling a rare task can improve its gradient contribution while encouraging memorization of a few examples; expand independent coverage before blindly repeating. Evaluate each important slice as well as the aggregate. Keep an ablation comparing the starting model and the same training recipe without a questionable data source.
