---
name: model-evaluation
description: Build regression evaluations for LLM answers, tool use and retrieval grounding; use for prompt/model comparisons and hallucination or task-completion scoring.
---

# Model Evaluation

Define success from actual user tasks and observable artifacts. Separate answer quality, tool execution, grounding, latency and cost; a fluent answer is not successful execution.

1. Build a small representative evaluation set with expected outcomes, failure cases and evidence references. Keep development examples separate from the held-out comparison set.
2. Use deterministic checks for exact facts, schema, code execution and task postconditions. Use judgment only where necessary; calibrate model judges against reviewed examples and preserve disagreements.
3. Include missing evidence, conflicting sources, injected instructions, unavailable tools and interrupted workflows. A correct refusal or explicit uncertainty can be the expected result.
4. Compare routes under matched tasks and declared sampling/retry budgets. Record actual model IDs, prompts, tool access and failures. Account for repeated trials when outputs vary.
5. Inspect regressions by category, not just aggregate score. Never replace live-system evidence with a mocked success claim; mocks verify the mocked boundary only.

Example: a browser agent saying “saved” fails if the record was never persisted. Score the persisted state, not the wording.

Deliver the runnable evaluation, failure examples and measured tradeoffs. Do not guarantee that passing the set proves general intelligence, factual perfection or production safety.
