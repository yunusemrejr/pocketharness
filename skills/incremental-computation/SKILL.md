---
name: incremental-computation
description: Engineer dependency graphs, memoization and incremental recomputation for builds, editors and derived state; use for invalidation correctness beyond cache tuning.
---

# Incremental Computation

Define the pure logical result first, then decide which parts may be reused. Incremental execution must agree with a fresh computation on the same logical inputs.

1. Identify every dependency affecting the result: content, configuration, tool versions, environment and relevant external state. Distinguish identity from version and value equality.
2. Define invalidation and propagation rules. Detect cycles explicitly or use an appropriate fixed-point algorithm when cycles are part of the semantics; do not silently process a cyclic graph as a DAG.
3. Commit dependency metadata and outputs consistently so interrupted work cannot publish a half-valid cache entry. Handle removed inputs and changed dependency sets.
4. Compare incremental results to clean recomputation across edit sequences: add, remove, rename, revert, configuration change and failed recompute. Test sequences, not merely one cache hit.
5. Measure reuse against the dependency-tracking cost. Prefer simple coarse invalidation when finer granularity adds complexity without worthwhile savings.

Example: caching compilation by source content alone ignores compiler flags. Reusing that artifact after a flag change can be fast and wrong.

Deliver the dependency/key contract, equivalence checks and measured reuse. One authoritative dependency owner is preferable to separate invalidation logic in every caller.
