---
name: distributed-systems
description: Design service coordination, retries, idempotency, queues, outbox flows, cache consistency and failure recovery. Use for multi-service or multi-node reliability.
---


# Distributed Systems

## First principle: reduce moving parts

Every network hop is a new failure class (latency, partition, replay, divergence). **The simplest design that meets the requirement wins** — one process + a queue beats five services for most products; "microservice" per table is an architecture tax. Split because you have: independent scaling (measured), independent deploy cadence (measured), or a hard isolation requirement (security/multi-tenant blast radius). Not because a slideware said so.

## The three siblings: timeout / retry / circuit break (the only correct order)

```
[call] ─timeout→ [retry w/ backoff+jitter (idempotent only)] ─budget→ [circuit breaker → fallback]
```

- **Timeouts everywhere:** DB, HTTP, DNS, queue ack — every single one. Rule: outer deadline ≤ sum of inner + margin (deadline propagation, e.g., gRPC context / `remaining time`), total request budget set at the edge. No timeout = no recovery, you hold resources forever.
- **Retries:** only **idempotent** ops (GET, PUT by key, POST with idempotency key), exponential backoff + jitter (uniform random × base; no fixed multiples — that's how thundering herds are built), bounded count (3) and bounded *budget* (≤ ~20–30% of normal traffic is retry traffic). Never retry 4xx except 408/429 (honor `Retry-After`)/429. A retry of a non-idempotent write = double-charge class bug.
- **Circuit breaker:** closed → open (after N failures) → half-open (one probe) → closed. **Failure is a state change, not an event.** Breaker = "stop hitting a dead dependency" (fail fast, free the pool, serve fallback). Not "fix the dependency" — nothing fixes that; it buys time.
- **Bulkhead:** separate pool/queue per dependency (a slow payments provider must not consume all your threads/connections — limit its pool, not "the pool"). Connection pool = your bulkhead; make the per-dependency sizes explicit.
- **Fallbacks:** stale cache (labeled "as of"), degraded feature (recommendations off → popular list), queue-and-later for non-urgent writes. A fallback must be *visible* as fallback (tells the user data may be stale) — a silent degradation is how "we've been showing stale prices for 6 hours" happens.

## Consistency: pick per data, not per religion

- **Strong / single-writer** (Postgres primary, etcd, one-service-owns-this-aggregate): money, inventory, auth state, anything two-writers break. This is 90% of real data.
- **Eventually-consistent** (replicas, Kafka feeds, CDNs): read-models, catalogs, settings, "nice to have fresh." State the staleness SLA ("within 30 s") and design the UI for it (see `databases` replica notes).
- **The anti-pattern** is "eventually consistent" as an *excuse* ("it'll catch up") with no bound — unbounded eventual = you've shipped a bug with a story.

## Idempotency & delivery semantics (the money part)

- **At-least-once delivery is the real world** (network retries, redeliveries, duplicate clicks). Design every consumer as **at-least-once + idempotent** and you get effectively-exactly-once for free. "Exactly-once" is a transport claim; *idempotent consumer* is the property you actually need.
- Idempotency key = natural (order id + event type) or explicit; consumer dedupes (insert-if-absent atomic, "seen" table with TTL, or `INSERT ... ON CONFLICT DO NOTHING`).
- **Dedupe window** must exceed redelivery horizon (an event redelivered in 2 h must still be a dupe, not a second payment).

## Queues (when the write is allowed to wait)

- Use a queue for: decoupling (producer/consumer evolve independently), buffering (spikes — burst 100×, consumers steady), and fan-out (one event, N subscribers). If none apply, keep it sync-synchronous (a queue adds: lag, ordering, poison messages, and an ops surface).
- **Ordering: per-key, never global.** Partition/partition-key by the entity (order id) and accept "parallel across keys." Anyone who needs *global* order has a throughput problem, not a queue problem.
- **At-least-once**: ack after *fully* processed (durable side effect done); crash between do and ack = redelivery = why consumers are idempotent.
- **Poison messages** (always fail): retry with backoff (5×), then **dead-letter queue** (DLQ) — alert on DLQ *non-empty*, not on "queue failing." DLQ + tooling to inspect/replay/reject is part of the queue, not a later project.
- Metrics that mean something: **depth**, **consumer lag (age of oldest un-consumed per partition)** — lag is the user-visible latency of the async path — and poison rate.
- **Outbox pattern (atomicity between DB and event):** instead of "write row, then publish event" (crash between = event lost OR event without row), **write row + outbox row in one DB txn**; a relay (poll/`LISTEN-NOTIFY` + read-committed tail) publishes and marks sent; consumers are idempotent. No 2PC, no distributed txn, no "eventually we'll fix the double write." This is the single highest-leverage pattern in event-driven backends.
- In Postgres, `SKIP LOCKED` claim loops are honestly a queue for modest volume (see `databases`) — a broker (Kafka/SQS) for cross-team/very-spiky/fan-out.

## Caching in the distributed (consistency, not hit-rate)

- Per cache key, decide: **stale-OK? for how long?** TTL = that budget + jitter.
- Invalidation: event-driven (write path emits invalidation via outbox → pub/sub → caches drop) for freshness-sensitive; TTL for the rest; both (event-driven + TTL backstop) for the small set where staleness would be user-visible.
- **Stampede:** single-flight (one in-flight fetch per key; joiners wait) or request-coalescing; jittered TTLs; hot keys get a short-TTL L1 per instance.
- Cache is not a database: no source-of-truth data lives *only* in the cache (memory, durability, "who is holding what"); rebuild = cheap (or you have the wrong architecture).

## Observability (you cannot debug what you cannot see)

- **3 signals, per scope:** RED for services (rate, errors, duration — the user-facing stuff), USE for resources (utilization, saturation, errors — the machine stuff), and the **business** metric (orders/minute, messages delivered). If an alert doesn't protect one of these, it's noise.
- **Tracing:** trace-id (W3C `traceparent`) generated at the edge, propagated through every hop (HTTP header, gRPC metadata, queue message attributes); spans per hop + per slow step within. This is the difference between "it was slow" and "the inventory call p99 was 4 s because of X."
- **Logging:** structured (JSON), 1 line per request/event with `request_id` + `trace_id` + `tenant` (log IDs, not PII — see `security`); **grep-able** = queryable. Errors logged at the boundary with context (what, input shape not value, error, retry count) — "error" with no context is a log, not data.
- **SLO + alerts:** SLO from the user (p95 API < 400 ms, 99.95% success) → error budget → **alert on symptoms** ("p95 > 400 ms for 5 min", "error rate > 1% for 10 min"), never on causes ("CPU 80%" alerts = a war with physics). **Every alert has a name + a runbook** (what to do, in order) — an alert with no runbook is a 3am panic, not an alert.
- Dashboards: the 5 that get you 90% (RED per service, queue lag/depth, error budget burn, dependency latency by provider, one business number). Start there.

## Failure classes (know them by name; each has a named fix)

| Class | Look | Fix |
|---|---|---|
| Cascade / pileup | everything slow at once, pools exhausted | timeout + breaker + bulkhead (no timeout = guaranteed cascade) |
| Thundering herd | spike on cache expiry / deploy | jittered TTL, single-flight, gradual deploy |
| Split-brain | two "leaders" write divergent state | leader lease with expiry + fencing tokens (old leader's writes rejected), or accept one-writer topology and audit for violations |
| Clock skew | "out-of-order" events, wrong ordering | don't order by client clocks; server-stamped, or hybrid-logical clocks where order truly matters |
| Retry storm | traffic multiplies under failure | retry budget, backoff+jitter, and "retry only on 5xx/network" discipline |
| Poison queue | one consumer stuck, lag climbs | DLQ after bounded retries; alert on lag age, not just depth |
| Bimodal latency | p50 fast, p95 broken | measure tails (histograms, not averages); one slow dependency hides in the average |

## Validation (the difference between "should work" and works)

- **Chaos in staging:** kill a random instance mid-traffic; inject 500 ms / 5 s latency into one dependency; fail the dep entirely (point it at a dead port); verify: no user-facing 500 storm, fallback visible, lag bounded, recovery automatic. (Chaos Monkey / a 10-line script + a health check is enough at most scales.)
- **Load at 2× peak p95** (k6), measure: tail latencies degrade gracefully (queues back up *bounded*), no OOM, error budget behavior.
- **"Turn off the dependency" drill** quarterly: each external provider, assume it's dead for 30 min — which user paths survive, which degrade, which die? The list of "die" paths is the roadmap.
- Integration tests run against **real deps** (testcontainers: real Postgres/Redis/queue, not fakes) — fake-backed tests green while the real thing is red is the classic CI/prod gap.

## Anti-patterns

Retry without idempotency (the money bug) · global ordering from a queue (throughput cliff) · "eventual consistency" with no bound · cache as source of truth · microservice per table · sync call chain of 5 (p99 is the product of 5 p99s) · 2PC across services (don't; outbox + sagas with **compensation** (undo steps) for multi-step business flows) · alerts on causes · "it works in staging" as an argument (staging without chaos has never told the truth) · a queue with no DLQ and no lag alert (that's a data graveyard).

## Detailed coverage

Distributed-systems engineering — timeouts/retries/circuit breakers/bulkheads done right, idempotency & exactly-once semantics, queues & the outbox pattern, consistency levels & which one per data, cache consistency (stampede, invalidation), observability (RED/USE, tracing, SLO-based alerts), failure classes (cascade, herd, split-brain) and chaos validation. Use when designing multi-service/multi-node systems, adding a queue or cache, or debugging "works locally, falls over in prod".
