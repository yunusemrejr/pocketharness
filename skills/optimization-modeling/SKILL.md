---
name: optimization-modeling
description: Formulate and validate constrained optimization, convex objectives, nonlinear solvers and discrete decision problems; use for advanced mathematical decision models.
---

# Optimization Modeling

Write down decision variables, domains, objective direction, constraints and units. Separate observed constants from assumed parameters. Confirm that the mathematical objective expresses the user's actual goal.

1. Construct a feasible tiny instance and a known infeasible one. Verify constraint signs and objective scaling by hand or exhaustive enumeration.
2. Determine which properties are established: convexity, differentiability, integrality, boundedness. Do not infer global optimality from a local solver's success flag.
3. Choose an installed solver that supports the actual domains. Inspect gradients/Jacobians, scaling and initialization where applicable; preserve solver status and termination reason.
4. Independently recompute feasibility violations and objective from the returned candidate. For discrete problems report the best bound/gap if available; for continuous problems inspect applicable optimality residuals and constraint qualifications.
5. Test sensitivity to meaningful parameter changes. Multiple starts may expose poor local solutions but do not prove global optimality.

Example: rounding a relaxed resource allocation can violate capacity. Recheck every constraint after rounding or solve the discrete problem directly.

Deliver the formulation, executable instance, candidate, feasibility tolerances and the strength of the optimality claim. State missing bounds or unverified assumptions explicitly.
