# Physics Modeling and Validation: patterns and examples

## Model before solver
List quantities with dimensions and choose a coherent unit system. Nondimensionalization exposes controlling ratios and conditioning. State whether the model is Newtonian, relativistic, quantum, continuum or discrete; a formula outside its approximation regime can be precisely computed and still wrong. Boundary conditions are part of the problem, not solver defaults.

For a harmonic oscillator, `m*x'' + k*x = 0`, `omega = sqrt(k/m)` and `E = (m*v² + k*x²)/2`. These give independent period and energy checks. A stable-looking trajectory with the wrong period is not validated. Symplectic methods can bound long-run energy error for suitable Hamiltonian systems but do not exactly conserve arbitrary energies or handle dissipation automatically.

## Domain-specific checks
Electromagnetic models must satisfy relevant divergence/charge-continuity constraints and material assumptions. Electrostatic, quasistatic and full-wave approximations have different regimes. Check dimensions and sign conventions in potentials/fields and circuit-source orientation. For thermal diffusion, test equilibrium, energy balance and timestep restrictions of the chosen explicit discretization.

Quantum state evolution needs normalization, Hermitian observables and unitary closed-system evolution. A density matrix must be Hermitian, trace one and positive semidefinite; a numerically convenient update can violate positivity. Classical probability and quantum amplitudes are not interchangeable. State the basis and units of Planck's constant before exponentiating a Hamiltonian.

## Numerical evidence
Use manufactured solutions, symmetry, zero/large-parameter limits and grid/timestep refinement. Estimate observed convergence order from errors against an independent reference; agreement between two coarse grids can hide a shared error. Distinguish chaotic trajectory divergence from statistical or invariant disagreement. Monte Carlo uncertainty needs independent effective samples, not just raw iteration count.

Report discretization, integrator tolerances, material parameters and uncertainty sources. Renderings should label transformed scales and illustrative effects. Verify consequential constants against authoritative references and never invent experimental measurements. For advanced derivations, identify the assumptions behind each identity and check a special case symbolically or numerically.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://physics.nist.gov/cuu/Constants/
- https://www.nist.gov/pml/special-publication-811
- https://www.damtp.cam.ac.uk/user/tong/teaching.html
