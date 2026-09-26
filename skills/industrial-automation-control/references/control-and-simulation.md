# Control logic and simulation invariants

## Define the plant and sample contract

Write the measured variable, desired variable, actuator command and their units before tuning. Record sensor scaling, sign convention, range, latency and sample period. A controller with correct mathematics can fail if a 4–20 mA signal is interpreted as 0–20 mA or a percentage output is treated as engineering units. Separate raw values, quality flags and converted values. An invalid measurement should enter an explicit fault policy rather than silently becoming zero.

For a sampled PI controller, one transparent formulation is `e[k] = r[k] - y[k]`, `u_raw[k] = Kp*e[k] + I[k]`, and `I[k+1] = I[k] + Ki*dt*e[k]`. Document that `Ki` has output-per-error-per-time units. Apply actuator bounds separately as `u = clamp(u_raw, u_min, u_max)`. This formulation alone winds up when the actuator saturates. Conditional integration can suspend accumulation that would drive further into saturation; back-calculation can feed the saturation difference into the integrator. Choose one defined method and test its transition behavior. The controller author's [anti-windup discussion](https://controlguru.com/integral-reset-windup-jacketing-logic-and-the-velocity-pi-form/) explains why output clipping alone is insufficient.

Derivative action amplifies noise. If used, define a filter, sample timing and whether differentiation applies to measurement to avoid a setpoint kick. Do not copy gains between positional and velocity forms without matching their definitions. Include bumpless transfer between manual and automatic modes, integral initialization and actuator rate limits. Scan-time variation and communications jitter are part of the plant-facing timing contract.

## Specify state transitions

Use explicit states such as stopped, ready, starting, running, stopping and faulted when the equipment requires sequencing. For each transition, specify prerequisites, timeout, commanded outputs and abort behavior. A reset acknowledges a fault; it should not implicitly restart motion unless that is the approved machine behavior. Define how restart, retained memory and power loss affect commands. Edge-triggered actions require a documented re-arm condition so a held button or retried message does not repeatedly actuate equipment.

## Test the abnormal cases

Simulate sensor disconnection, frozen readings, wrong sign, maximum delay, actuator saturation, failed feedback and interrupted sequencing. Check bounds and rates on every step, not just the final trajectory. Compare timestep refinements so numerical artifacts are not mistaken for a stable design. Include disturbances and parameter uncertainty, and record overshoot, settling criterion and worst-case command. A visually smooth plot is not a safety argument.

Use the process's identified hazards to select invariants. For example, mutually exclusive actuators must not energize together, and a permissive must remain valid throughout a hazardous action when required. [NIST SP 800-82 Rev. 3](https://csrc.nist.gov/pubs/sp/800/82/r3/final) explains why OT must account for physical reliability and safety constraints. Preserve independent safety functions and validate on the appropriate test bench before operational commissioning.
