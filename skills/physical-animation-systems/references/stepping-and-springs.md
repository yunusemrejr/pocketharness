# Stepping and springs

## One clock owns simulation

Use seconds for numerical time. Accumulate elapsed presentation time; while the accumulator is at least fixed step h, copy current to previous, simulate h, and subtract h. Render an interpolation of previous/current with alpha=remaining/h. This introduces one simulation-step presentation delay. Rotations need quaternion interpolation; positions can lerp. Never feed interpolated display state back into physics. This accumulator/interpolation method follows [Fiedler's original explanation](https://gafferongames.com/post/fix_your_timestep/).

Worked schedule: h=1/120 s, previous remainder=0.003 s, frame elapsed=0.019 s. Two steps consume 0.0166667 s; remainder=0.0053333 s and alpha=0.64. A 0.5 s stall would require 60 steps. Explicitly choose an overload policy: pause/resume, capped catch-up with reported dropped simulation time, or a bounded backlog. If dropping whole backlog steps, keep only its fractional remainder before computing alpha; clamping alpha while leaving an ever-growing backlog hides the failure. Offline deterministic export should advance frame timestamps exactly and never silently discard time.

Initialize previous=current. Reset both on teleport; otherwise interpolation draws an unintended sweep. Timestamp input and replay it at simulation boundaries. Separate fixed-rate reproducibility from bitwise determinism: iteration order, random seeds, SIMD and floating-point platforms also matter.

## Spring parameters with units

For displacement y=x-target, m*y''+c*y'+k*y=0:

- omega=2*pi*f, k=m*omega², c=2*zeta*m*omega.
- f is undamped natural frequency in Hz; omega is rad/s. For 0<zeta<1, observed oscillation frequency is f*sqrt(1-zeta²).
- k is N/m and c is kg/s when x is meters. A pixel spring can use consistent abstract units, but do not call its acceleration m/s².

Example derived from this ODE: m=2 kg, f=3 Hz, zeta=0.7 gives omega=18.8496 rad/s, k=710.6115 N/m, c=52.7788 kg/s. An initial 0.1 m displacement with zero velocity accelerates at -35.5306 m/s². The underdamped envelope decays as exp(-zeta*omega*t); reaching 2% of that envelope takes ln(50)/(zeta*omega)=0.2965 s. This envelope estimate is not an exact settling time for arbitrary initial velocity.

For a fixed target and critical damping zeta=1, exact evolution is:

```
b = v0 + omega*y0
E = exp(-omega*h)
y1 = (y0 + b*h)*E
v1 = (v0 - omega*b*h)*E
```

For omega=10, y0=1, v0=0, h=0.1: y1=2/e=0.7357589; v1=-10/e=-3.6787944. This formula tolerates large h mathematically; a moving target is only piecewise constant under this update. A target teleported each frame can inject energy. For moving attachments, use consistent target velocity/acceleration or a physically defined driver.

## Stability is not accuracy

For an undamped oscillator, explicit Euler has eigenvalue magnitude sqrt(1+(h*omega)²)>1 and gains energy. Velocity-first semi-implicit Euler has bounded oscillatory solutions for 0<h*omega<2 (the endpoint is unsafe). That bound is not a universal rule for damped, nonlinear or constrained systems. Even stable steps can distort phase. At 3 Hz and h=1/120, h*omega=0.1571; roughly 40 samples per cycle. Verify convergence by halving h and comparing phase/amplitude over the actual duration.

Implicit methods improve stiff-system stability but can damp away intended motion. Prefer analytical springs for isolated visual followers; use coupled solvers for interacting mass systems. Substeps advance time; solver iterations improve a solve at the same time. They are not interchangeable. Engine-specific damping and substep controls should follow its installed-version API; see [Box2D's official simulation guide](https://box2d.org/documentation/md_simulation.html).

For mixed cloth/rigid/particle rates, choose integer subdivisions of a common tick and exchange impulses at well-defined boundaries. Applying a force F in each substep gives total impulse F*h over the parent step; applying a parent-step impulse at every substep multiplies momentum incorrectly. Avoid applying both a parent transform and its displacement again to attached children.
