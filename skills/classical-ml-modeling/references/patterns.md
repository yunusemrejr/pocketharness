# Classical ML Modeling: patterns and examples

## Leakage-safe pipeline
```python
from sklearn.pipeline import make_pipeline
from sklearn.impute import SimpleImputer
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
model = make_pipeline(SimpleImputer(strategy="median"),
                      StandardScaler(), LogisticRegression(max_iter=1000))
# Fit only on the training partition; evaluate separately on untouched data.
```
This sketch assumes numeric features and a classification target. Categorical encoding, missingness semantics and class weighting need an explicit data contract. A Pipeline helps prevent preprocessing leakage but cannot fix an invalid random split across the same person or future information already present in features.

## Model selection
Compare linear, tree-based and simple nonparametric baselines appropriate to dimensionality and sample size. Scaling matters for distance/regularization methods but is not a universal requirement for trees. Tune hyperparameters with group/time-aware folds where necessary. Use nested validation or a separate final holdout when reporting after extensive search. Keep the test set out of feature selection, calibration and threshold choice.

For imbalanced classification, evaluate precision/recall and decision cost at the operating threshold rather than accuracy alone. ROC AUC can look strong while rare-event precision is unusable. Calibrate on independent validation predictions; a probability-looking score is not automatically calibrated. For regression inspect residuals by magnitude, time and group, and check whether target transformation changes the optimized error.

## Interpretation and operations
Feature importance is model-dependent association, not a causal effect. Correlated features can split or hide importance; permutation tests must respect data dependence. Clusters need stability and domain usefulness, not just a attractive projection. Record schema, units, category handling and training-data version with the model. Test unknown categories, all-missing columns, out-of-range values and changed feature order. Monitor input and outcome drift, but distinguish drift from proven quality loss. Report uncertainty and realistic baseline comparisons; never substitute training scores for generalization.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://scikit-learn.org/stable/common_pitfalls.html
- https://scikit-learn.org/stable/modules/cross_validation.html
- https://scikit-learn.org/stable/modules/calibration.html
