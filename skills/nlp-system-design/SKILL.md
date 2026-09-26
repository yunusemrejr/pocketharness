---
name: nlp-system-design
description: "Build multilingual NLP pipelines for retrieval, classification, extraction and text evaluation."
---

# NLP System Design

Use for language data and NLP systems, including non-LLM approaches. Preserve text provenance and language-specific behavior.

## Working method

- Define language, unit of prediction, labels and annotation policy.
- Preserve raw text and map normalization/tokenization offsets.
- Split by document/source/entity and establish a simple baseline.
- Evaluate relevant slices, uncertainty and downstream errors.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
