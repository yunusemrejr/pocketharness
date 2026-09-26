---
name: industrial-automation-control
description: Design and review PLC, SCADA and industrial control logic using process constraints, state machines, discrete control calculations, simulation and safe commissioning evidence.
---

Begin with the physical process, controller, sensors, actuators, engineering units and operating envelope. Distinguish offline design, simulator testing, read-only observation and commands that affect equipment. Preserve authorization already given, but do not treat access to a controller as permission to energize machinery or change a live process.

Read [control-and-simulation](references/control-and-simulation.md) for discrete PI/PID behavior, saturation, scan timing, state transitions and validation. Read [ot-integration-and-commissioning](references/ot-integration-and-commissioning.md) for PLC/SCADA interfaces, data quality, communications failure and staged deployment.

Use passive records or a test bench by default when the operational scope is not established. Live changes need the plant's actual operational boundary, responsible operator and applicable change procedure. Never bypass interlocks, force safety outputs or treat ordinary application logic as a substitute for a required independent safety function.

Make failure behavior explicit: stale sensors, lost communications, invalid units, controller restart, actuator saturation and emergency-stop recovery. A safe state depends on the process; de-energizing everything is not universally safe. Represent assumptions in the simulation and test matrix rather than burying them in comments.

Deliver logic, parameter units, invariants, test evidence and commissioning conditions. A simulation result supports a design decision but does not certify functional safety or demonstrate correct behavior on untested hardware.
