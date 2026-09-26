---
name: concurrency-memory-models
description: Review shared-memory threading, atomics, synchronization and linearizability; use for races, deadlocks or lock-free code rather than ordinary service retries.
---

# Concurrency Memory Models

Map each shared object to its synchronization owner. Prefer established locks, channels and concurrent containers unless a measured bottleneck justifies custom atomic code.

1. Identify conflicting accesses and the happens-before relationship protecting them. Atomicity of one field does not make a compound operation atomic.
2. Write down lock ordering, callback/reentrancy boundaries and cancellation behavior. Do not hold locks across unknown callbacks or suspension without analyzing the resulting dependency cycle.
3. For an atomic algorithm, state its linearization point and the required ordering for each operation, including failure ordering where relevant. Address publication, lifetime/reclamation and ABA separately.
4. Use appropriate race detectors or schedule exploration when available. Test cancellation, contention and cleanup; x86 success does not establish correctness on weaker memory models.
5. Distinguish deadlock freedom, lock freedom, wait freedom and fairness. Do not claim a stronger progress guarantee than the algorithm establishes.

Example: an atomic ready flag is insufficient if the publication protocol does not order the payload writes before the reader accesses them. Inspect the language memory model rather than borrowing assumptions from another runtime.

Deliver the synchronization argument, failure schedules and tool evidence. For Rust consult the [atomic ordering reference](https://doc.rust-lang.org/nomicon/atomics.html). Distributed service consistency belongs to distributed-systems.
