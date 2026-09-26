---
name: numerical-computing
description: Implement or diagnose floating-point calculations, linear algebra, gradients and numerical integration; use when precision, conditioning or convergence matters.
---

# Numerical Computing

Identify variables, shapes, units, domain and required error tolerance before choosing an algorithm. Distinguish a mathematically valid formula from a stable numerical implementation.

1. Build a tiny case with an analytic or independently computed answer. Check dimensions and limiting cases first.
2. Scale inputs; inspect conditioning and dtype. Solve linear systems rather than explicitly inverting matrices. Avoid subtracting nearly equal quantities when a stable formulation exists.
3. Check absolute and relative residuals. Near zero, relative error alone is misleading. Reject nonfinite intermediate values at their origin rather than hiding them with clipping.
4. Validate derivatives with directional finite differences over several step sizes at smooth interior points. Autodiff agrees with the implemented expression, not necessarily the intended mathematics.
5. Repeat with tighter tolerances, smaller steps or higher precision. Compare convergence, not just one attractive plot. Report the precision and tolerance actually used.

Example: a linear solver can have a small residual yet a large solution error on an ill-conditioned matrix. Test sensitivity to a small input perturbation before claiming accuracy.

Read installed solver documentation for its tolerance semantics. Deliver executable checks, error scale and remaining conditioning limitations.
