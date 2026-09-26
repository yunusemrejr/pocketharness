---
name: behavioral-contracts
description: Specify and test state transitions, invariants, idempotency and lifecycle behavior in retries, queues, agents or workflows; use for logic bugs beyond ordinary code style.
---

# Behavioral Contracts

Start from one concrete sequence that currently fails. Identify the state owner, inputs/events, allowed transitions and externally visible outcome. Keep the model small enough to inspect; do not create another runtime state store.

1. Write the invariant in observable terms: “one accepted job has at most one terminal result,” not “the scheduler is robust.” Separate safety (never duplicates) from liveness (eventually finishes under stated assumptions).
2. Enumerate the relevant orderings: completion before cancellation, cancellation before completion, late result after restart, repeated submission. Decide which event wins and who owns cleanup.
3. Reproduce with the actual lifecycle and deterministic time/events. A mock of the broken coordinator cannot validate that coordinator.
4. Add a counterexample test and, when many orderings matter, generated transition sequences with an independent invariant check. Preserve failing seeds.
5. Stop when the regression and relevant neighboring transitions pass. Document assumptions about external delivery and fairness; a finite test is not a proof of all schedules.

Example: a timed-out request may already have committed. Retrying is correct only with deduplication or a status reconciliation step. Never treat a timeout as proof of nonexecution.

Deliver the changed owner, the reproduced sequence, and the verified invariant.
