---
name: evidence-first-engineering
description: Use for multi-step code, infrastructure or utility changes where scope, duplication, verification or environment safety matters. Small local fixes should stay small.
---

# Evidence-first engineering

Explicit user requirements override these defaults. Do not reinterpret a vague request as authorization for a redesign, deployment or data migration.

1. **Recover intent.** Identify the current outcome, hard constraints and completion checks. Use `checkpoint_read` for exact earlier instructions when needed; later corrections supersede earlier ones only within their scope. Track substantial work in the existing todo/mission system, not a new ledger. Keep source references for disputed requirements. Ask only when ambiguity changes the action or its authority.
2. **Find the owner.** Start with project facts, symbols and existing tests. Read the relevant implementation before editing. Extend the established owner rather than adding a parallel cache, manager, wrapper, configuration store or abstraction. Prefer the smallest general fix; do not refactor unrelated code or conceal uncertainty behind fallback behavior.
3. **Protect other work.** Inspect dirty state and concurrent writers. Conversation forks are not file isolation; sibling notices are not locks. Use native worktree isolation or one writer per workspace. Never reset, stash, overwrite or commit another session's changes merely to obtain a clean baseline.
4. **Escalate risk, not ceremony.** For remote/deployment/synchronization work, inspect `project_report {view:"workspace"}` when available. Establish source, destination, authority, protected data, verification and rollback before mutation. Git, local and production are not interchangeable. Do not connect to production, enter credentials, synchronize or merge databases/uploads without task authorization. Reading and using existing credentials for an authorized task needs no additional per-use approval; avoid exposing secret values in logs or reports.
5. **Verify the claim.** Use focused diagnostics/tests and inspect the actual result. Run long work through background tasks; consume terminal notifications rather than polling. Before completion, distinguish requirements that are satisfied, unverified, unsatisfied, superseded or outside authorization. A test passing is evidence for its covered behavior, not proof of every requirement. Report blockers and residual risks honestly; remove development-only scaffolding.

For uncertain work, choose the next check that can change the implementation decision. Small reversible edits need a small check; concurrency, data loss and public contracts deserve failure-path evidence. Do not equate reasoning length with quality or keep exploring after the relevant checks pass.

## Double-check protocol (before claiming done)

When asked to double-check, or before a completion claim on consequential work:
1. List every claim the answer makes (fixed, faster, compatible, total = X, all tests pass) and the evidence for each; a claim without evidence is marked unverified or removed.
2. Re-derive, do not re-read: rerun the test or build after the last edit, recompute key numbers a second way, re-render UI, re-query data. Reading your own diff again is not verification.
3. Attack the result: the negative case, the boundary, empty input, the other platform or locale, the concurrent path. Try to make it fail once.
4. Check the request again: every explicit requirement and constraint from all user messages and reminders is met, or its gap is stated.
5. Compare against the original baseline: nothing unrelated changed (diff scope, public contracts, config, generated files).

For data/API claims, inspect actual schema, units, nulls, keys and installed versions. Calculate consequential totals in executable code; validate join cardinality and denominators. Keep observations, calculations and assumptions distinct. Record the source or command behind a claim; missing evidence stays unknown. Mocked success is not live-service validation.
