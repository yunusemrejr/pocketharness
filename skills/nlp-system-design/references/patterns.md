# NLP System Design: patterns and examples

## Text contracts
Unicode code points, grapheme clusters and bytes are different units. Normalization may alter offsets; retain source text and an explicit mapping if annotations refer to spans. Case folding and tokenization are language-dependent—Turkish dotted/dotless I is a useful counterexample to English assumptions. Do not strip accents, punctuation or stop words automatically: they may carry label or identity information.

For sequence labeling define whether spans are inclusive/exclusive and measured in bytes, characters or tokens. Overlapping/nested entities need a representation beyond a single BIO sequence. Validate round-trip spans against original text, including combining marks and emoji. Annotation disagreement is evidence about task ambiguity, not always careless labeling.

## Baselines and retrieval
Start classification with an appropriate sparse baseline such as TF-IDF plus a linear model. Fit vectorizers only on training data. For retrieval compare lexical, dense and hybrid methods using query-level held-out sets. A combination such as reciprocal rank fusion uses ranks rather than incomparable raw scores: `score(d) = Σ 1/(k + rank_i(d))`; choose/tune k without leaking test relevance. Evaluate recall at the retrieval stage and ranking quality separately.

Chunking must preserve document identity, boundaries and useful context. Deduplicate near copies across splits; otherwise retrieval and classification scores can be misleading. For extraction, distinguish exact-match, overlap and entity-level metrics rather than choosing the most flattering number.

## Production behavior
Report macro/micro metrics and per-language or rare-class behavior appropriate to the task. Calibrate confidence on held-out data and define abstention when errors are costly. Monitor vocabulary/domain drift and annotation policy changes. Tokenizer/model versions are part of the deployed artifact. Tests should include empty text, malformed encodings, mixed languages, long documents, negation and realistic typos. Generated summaries require source-grounding checks; semantic similarity alone does not prove factual accuracy.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.unicode.org/reports/tr15/
- https://scikit-learn.org/stable/tutorial/text_analytics/working_with_text_data.html
- https://huggingface.co/docs/tokenizers/
