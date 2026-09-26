# Evidence-based decision routing

The [Just Ask Jev paper, v1](https://arxiv.org/pdf/2609.29429v1) evaluates batched typed questions and probability readouts. Its distinction between question wording and available state matters: relational judgments need the relevant evidence. It also finds that probability thresholds do not transfer reliably across benchmark domains. Label-defining references can inflate apparent improvements, and fitting a threshold on a few labels can hurt held-out results. The paper's benchmark accuracy and cost ratios are not PocketHarness measurements.

## PocketHarness contract

- Batch related yes/no criteria about one state. Keep the user's request, actual observations and final answer distinct; do not add a gold label or judge verdict to production evidence.
- Remote criterion instructions identify the state as untrusted evidence. This is a defense, not proof against prompt injection. Preserve structured references when routing a transcript to Jev.
- Keep continuous returned probabilities. Missing, malformed or nonfinite scores are unknown, not zero. Fixed thresholds express routing policy, not certified error rates.
- Transcript quality criteria `premature`, `ignored` and `bad` share one evidence-sufficiency question in the same remote request. Its policy requires at least 0.85 support. Missing or uncertain support removes those quality answers; normal verification/review still runs. A tiny local model cannot supply this certification.
- Local action/content hints and the existing 1.5-second local-only progress check remain advisory. Do not send a remote request after every tool call or use a decision score alone to declare a goal complete.
- Cache exact state and question inputs. Never reuse scores after the evidence changes. API failures are not cached approvals.

## Optional offline threshold check

The [C++17 evaluator](../assets/decision_threshold.cpp) reads saved probability/label pairs; it makes no network requests and changes no harness configuration. Its explicit example loss charges two units per false alarm and one per miss. Choose the actual product cost before comparing policies.

From this skill's directory:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Werror assets/decision_threshold.cpp -o "$TMPDIR/decision-threshold"
"$TMPDIR/decision-threshold" assets/threshold-calibration.csv assets/threshold-heldout.csv
```

The [calibration fixture](../assets/threshold-calibration.csv) and [held-out fixture](../assets/threshold-heldout.csv) are invented, labelled demonstration scores, not Jev outputs or a quality benchmark. The fitted threshold deliberately performs worse on held-out data. Unknown scores use `-1` and reduce coverage; they are excluded from score-quality metrics, never counted as negative predictions. Output includes the confusion matrix, coverage, Brier score and ten-bin ECE. On nine held-out rows these estimates are highly uncertain, especially ECE.

For a real task, pin the model/version, criteria and state schema. Collect independent human-reviewed examples, including unavailable evidence and ambiguous cases. Group related cases into separate calibration and held-out partitions before choosing wording or thresholds. Fit only on calibration data, freeze the policy, then report held-out false positives, misses, coverage and raw-score reliability. Keep labels outside requests sent to the judge. Audit confident disagreements before assuming either source is correct. A lower training loss or a small fixture pass does not justify deploying a new threshold; retain normal review when evidence is weak.
