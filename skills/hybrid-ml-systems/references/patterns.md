# Hybrid ML Systems: patterns and examples

## Choose the composition
A cascade uses a cheap stage to filter/route; an ensemble combines predictions; stacking learns a combiner; a rule layer enforces domain constraints. These are different architectures. Define whether a rule is a hard invariant or a heuristic. Do not let a heuristic silently overwrite a calibrated probability while still calling it calibrated.

For stacking, train the meta-model on out-of-fold predictions from base models. Training it on predictions made on those models' own training rows leaks information. Preserve group/time splits throughout every stage. At inference, feature and score schemas must match training, including missing-stage behavior.

## Example decision cascade
A lightweight classifier handles high-confidence routine cases; uncertain cases go to a neural model; an explicit validator rejects invalid outputs. Choose the routing threshold on validation data using total error cost and latency, not a guessed confidence number. Measure coverage and error rate of the cheap stage separately, and evaluate the hard routed subset rather than only the overall average. A fallback trained on easy examples may fail precisely where it is needed.

For residual modeling, `prediction = baseline(x) + residual_model(x)`, ensure residual targets are formed without fitting the baseline on the evaluation rows. For physics-informed models, dimensional consistency and boundary conditions are contracts, not merely extra loss terms. Report physical residual and predictive error separately.

## Reliability and ablation
Remove one component at a time to determine whether it contributes beyond complexity. Test correlated errors, unavailable upstream models, stale features and version skew. Propagate uncertainty deliberately; multiplying probabilities assumes relationships that may not hold. Avoid a circular feedback loop where pseudo-labels become unquestioned ground truth.

Package component versions, preprocessing and routing policy together. Measure end-to-end tail latency, memory and cost, including fallback frequency. Keep logs sufficient to trace which stage made a decision without exposing sensitive data. A simpler single model that meets the actual objective is preferable to an ensemble with no demonstrated benefit.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://scikit-learn.org/stable/modules/ensemble.html
- https://scikit-learn.org/stable/modules/generated/sklearn.ensemble.StackingClassifier.html
- https://pytorch.org/docs/stable/
