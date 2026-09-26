---
name: formal-model-checking
description: Build small executable formal models for safety, liveness and protocol properties; use when explicitly modeling or proving behavior beyond lifecycle regression tests.
---

# Formal Model Checking

Choose a property whose failure has a concrete consequence. Model the smallest system that can violate it; do not add formal methods ceremony to a trivial edit.

1. Define state, initial conditions, transitions and environment assumptions. Specify what is abstracted away and how model actions map to implementation actions.
2. Separate safety from liveness. State fairness and failure assumptions needed for eventual progress; do not obtain a passing result by assuming away the failure being investigated.
3. Run an available checker on finite bounds first. Validate the model by introducing a known defect and confirming the property detects it; inspect reachability to avoid vacuous success.
4. Convert counterexample traces into implementation regressions. If the model passes, report explored bounds, properties, tool/version and whether exploration was complete or resource-limited.
5. Distinguish bounded checking, inductive proof and testing. A verified abstract model does not prove that the implementation faithfully refines it.

Example: “no two workers own a job” can hold while no worker ever receives one. Check progress separately under the actual scheduling assumptions.

Deliver the model, reproducible command, results and abstraction limits. If no checker ran, label the result an unverified model or reasoning sketch, never a proof. Use behavioral-contracts for ordinary lifecycle tests.
