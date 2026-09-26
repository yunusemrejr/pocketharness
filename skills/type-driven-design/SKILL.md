---
name: type-driven-design
description: Encode domain constraints with algebraic data types, typestate, exhaustive handling and validated boundaries; use when invalid states or ambiguous null/boolean combinations cause bugs.
---

# Type Driven Design

Start from the real domain states and transitions. Improve the touched boundary rather than rewriting the codebase to showcase a type system.

1. Separate raw external data from validated domain values. Parse once at the trust boundary and preserve the validated representation internally.
2. Replace incompatible boolean combinations with explicit variants when it prevents actual invalid states. Distinguish absent, unknown, empty and failed values only where their behavior differs.
3. Make variant handling exhaustive using the language's supported mechanism. Keep errors actionable and distinguish expected domain failures from programmer defects.
4. Use units, identifiers or lifetime/state wrappers when mixing them would cause a concrete bug. Avoid deep generic machinery that costs more to understand than the invariant it enforces.
5. Verify both compile-time examples and runtime boundary cases. Casts, reflection, deserialization, unchecked code and external storage can bypass static guarantees.

Example: `{loaded: true, error: true, data: null}` permits contradictory states. A loading/ready/failed sum type constrains internal code, while the network response still requires runtime validation.

Deliver the invalid state removed, the simpler API, and tests for inputs the compiler cannot verify. Type-checking success is not proof of business correctness or external data validity.
