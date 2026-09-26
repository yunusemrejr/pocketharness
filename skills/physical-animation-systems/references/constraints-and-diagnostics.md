# Constraints and diagnostics

## XPBD compliance

For scalar constraint C, inverse masses wi, compliance a and substep h:

```
aTilde = a / h²
DeltaLambda = (-C - aTilde*lambda) /
              (sum(wi * |gradient_i C|²) + aTilde)
DeltaXi = wi * gradient_i C * DeltaLambda
lambda += DeltaLambda
```

Initialize lambda=0 at the start of the substep in the basic algorithm; accumulate across iterations within it. Warm starts need deliberate timestep scaling and contact lifetime management. Use the actual substep duration, not display delta. This is the scalar form of [Macklin, Müller and Chentanez, XPBD, equations 18–19](https://matthias-research.github.io/pages/publications/XPBD.pdf). Compliance removes a major timestep dependency in stiffness parameterization; finite iteration count, discretization and nonlinear solves still affect the result.

Derived distance example: masses 1 kg each, rest length 1 m, current distance 1.1 m, a=0.0001 m/N, h=1/60 s. Unit opposite gradients give denominator 2+0.36. Initial DeltaLambda=-0.1/2.36=-0.0423729 kg*m. Each end moves 0.0423729 m inward; residual stretch is 0.0152542 m. At h=1/120, aTilde=1.44; reusing 0.36 would accidentally change the model. Fixed particles have wi=0. Coincident endpoints have undefined distance gradients: retain a known direction or handle that degeneracy explicitly, never divide by zero. Both endpoints fixed with zero compliance has zero denominator and should produce no correction.

## Contact is unilateral

Choose normal orientation and relative-velocity sign once. For two nonrotating particles, approaching normal speed vn<0, normal impulse j=-(1+e)*vn/(wA+wB); separating contacts receive no restitution impulse. With vn=-3 m/s, masses 1 kg and 2 kg, e=0.5: j=3 N*s and outgoing relative normal speed=1.5 m/s. For rigid bodies include angular effective mass from contact offsets and world inverse inertia; the particle formula cannot model off-center impacts.

Clamp accumulated normal impulses nonnegative. Coulomb friction bounds tangential impulse magnitude by mu*jNormal; independent component clamps form a square rather than the intended disk in 3D. Static contact can have zero tangential speed and nonzero friction impulse. Restitution near rest needs a velocity threshold to avoid jitter; a threshold is numerical/art-directed policy, not conservation. Fast thin objects require continuous collision detection or swept tests; extra solver iterations alone do not detect missed contacts.

## Measure the right invariant

Track finite state, maximum constraint residual, penetration, kinetic/potential energy, total linear momentum and angular momentum about a stated origin. Compare tolerances in scene units and over time, not just one frame.

For isolated conservative mechanics, energy drift should shrink with a suitable refined discretization; symplectic methods generally bound energy error rather than preserve energy exactly. Damping, friction and inelastic impacts dissipate energy. External forces, moving anchors, motors and scripted targets can supply energy/momentum. Static walls exchange momentum with bodies, so the bodies alone are not an isolated system. Position projection can alter momentum; quantify rather than hide that correction.

A useful diagnostic residual is DeltaE - externalWork + dissipatedEnergy, using a consistent numerical quadrature. Do not demand this residual be exactly zero from an approximate solver. For a falling body include m*g*y potential with your up-axis sign. For an isolated elastic two-body impact, test both momentum and kinetic energy; a visually plausible bounce can pass one and fail the other.

For an art-directed squash/stretch preserve volume only when the material story requires it. If scale along x is s and isotropic transverse scale is r, volume preservation means s*r²=1, so r=1/sqrt(s). At s=1.44, r=0.833333. Nonuniform parent scales and skinning can defeat that local calculation; inspect world-space deformation.
