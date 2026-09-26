---
name: simulation-engineering
description: Build and validate physical, agent-based, stochastic or interactive simulations with reproducible stepping, calibration and conservation checks.
---

# Simulation Engineering

Specify the system boundary, state variables, units, governing rules, initial/boundary conditions and intended use. Distinguish an explanatory animation from a predictive simulation.

1. Separate the simulation state/step from rendering and controls. Advance simulated time explicitly; frame rate must not change the physics. Keep random generators seedable and record configuration.
2. Build a tiny reference scenario with a known equilibrium, analytic solution or independently checked outcome. Validate conservation or other domain invariants before making the visuals elaborate.
3. Examine stability and convergence across time steps, solver tolerances, grid sizes or particle counts as appropriate. A smooth animation can conceal numerical instability.
4. For stochastic systems run independent replications and quantify variation. Reproducible seeds support debugging; one seed is not evidence of general behavior.
5. Calibrate against data separately from validation. Track parameter identifiability and mismatch; do not tune to the validation cases and then call them independent evidence.

Example: drive a pendulum from fixed simulation steps, interpolate rendering, and compare energy drift as the step shrinks. Select an integrator appropriate to the actual dynamics rather than applying this example universally.

Deliver equations/rules, runnable scenario, pause/reset controls when interactive, validation outputs and applicability limits. Never label an uncalibrated visual demo a scientifically validated model.
