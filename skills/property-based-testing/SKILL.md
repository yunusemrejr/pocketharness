---
name: property-based-testing
description: Design generative, differential, metamorphic and fuzz tests with independent oracles and shrinking; use for broad input spaces that examples under-cover.
---

# Property Based Testing

Choose an observable property and state its valid input domain. A property that repeats the implementation is not an independent check.

1. Start with a tiny trusted oracle or a relation that must hold. Examples include comparing optimized output to brute force or verifying an operation preserves a multiset.
2. Generate valid structured inputs directly when filtering would discard most samples. Deliberately cover boundaries and separately generate malformed inputs for rejection behavior.
3. Check that the property rejects a known faulty implementation. Round-trip tests alone can miss paired encoder/decoder errors, so include independently known values.
4. Shrink failures while preserving the conditions that made them meaningful. Save minimal counterexamples and seeds as ordinary regression cases.
5. Bound time, memory and side effects. Fuzz untrusted parsers in an appropriate isolated process, not against production endpoints. Separate hangs, crashes and semantic mismatches.

Example: a sorting property checking only adjacent order accepts an implementation returning an empty list. Also verify length and element multiplicity, and compare small cases with a reference sort.

Deliver the property rationale, generator/domain, discovered counterexamples and executed budget. Passing a finite campaign supports the covered cases; it does not prove correctness for every possible input.
