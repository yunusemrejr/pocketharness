---
name: databases
description: Design schemas, indexes, transactions and safe migrations; diagnose queries and data integrity. Use for SQL/NoSQL workload choices and database correctness or performance.
---


# Databases

## Choose by workload (start relational)

- **Postgres for 95% of apps**: SQL + JSONB + FTS + pgvector + H store in one; if you outgrow Postgres you've likely hit a real scale wall worth a serious redesign, not a store-swap.
- NoSQL (doc: Mongo; wide: Cassandra; KV: Redis/Dynamo) when: data is truly wide/semi-structured, scale-out write throughput, or KV patterns (cache/session/leaderboard). Costs: weaker consistency (understand the mode you're in), query flexibility, and ops. "I heard SQL doesn't scale" is not a reason.
- Purpose-built: time-series (TimescaleDB/ClickHouse) at real volume · search (Postgres FTS → Meilisearch/OpenSearch when FTS is slow) · vector (pgvector first — good to tens of millions; dedicated when retrieval is the product) · cache (Redis) — but it's *cache*, not storage.
- Data classes: PII/PCI → least-privileged, audited, maybe separate cluster + column encryption; analytics → replica/warehouse, never the txn DB.

## Schema design (OLTP)

- **Normalize to ~3NF** (remove redundant facts, single source per fact); denormalize a proven *hot read* with a view/materialized view — never by hand-synced columns.
- **PKs:** `bigint identity` (or `bigserial`) for most; `uuidv7` (time-ordered) for anything appearing in URLs/scale-out — never `uuidv4` as clustered PK (random = index churn + bloat).
- **FKs always** (DB-enforced) with explicit `ON DELETE` (RESTRICT vs CASCADE — say which, per pair).
- Every column: type (smallest honest type; `timestamptz` for time — UTC internally, format locally), NOT NULL when there's a default, NULL documented as a meaning (or don't use it).
- **JSONB or columns**: columns when you filter/sort/index/join; JSONB when shape varies and you select whole. A JSONB field you query often is a table you didn't extract.
- Enums: `CREATE TYPE` (or a lookup table) — not free-text; store code (`"open"`), display in the app.
- Money: `numeric` (never float), state currency explicitly.
- Table naming plural, column naming consistent (snake_case); one convention written down.

## Indexing & query work

- **Index every `WHERE`, `JOIN`, `ORDER BY`, `GROUP BY` pattern that appears in hot queries.** Composite: equality cols first, then range, then sort (`(tenant_id, status, created_at)`). Partial indexes for hot subsets (`WHERE deleted_at IS NULL`) — smaller, faster. GIN for jsonb/arrays/fts; BRIN for monotonic (time) large tables.
- **`EXPLAIN (ANALYZE, BUFFERS)` is a reflex** for any suspicious query: `Seq Scan` on a big table with a filter = missing index; `Nested Loop` over large sets = bad plan; `Buffer` hit ratio = is it in cache?
- Index usage is measurable: `pg_stat_user_indexes` (idx_scan=0 on a hot table = delete candidate; disk-space + write-cost tax for free indexes).
- **N+1 is a query bug, not a cache excuse:** batch-load (single `IN (...)`, or a data-loader pattern in the app, or JOIN). If you're doing 10k selects for one page, no cache tier survives that shape for long.
- Slow-query log: `log_min_duration_statement = 100ms` (tune), review weekly; top-5 offenders is the honest performance roadmap.
- Pagination: **keyset** (`WHERE created_at < $last ORDER BY created_at DESC LIMIT 50`) instead of `OFFSET 100000` (offset = scan + discard).

## Transactions & concurrency (where correctness lives)

- Default Postgres isolation **READ COMMITTED** is correct for 95%; use **SERIALIZABLE** (or SELECT...FOR UPDATE) where concurrent writes can break invariants (account balance, stock, seat allocation) — and handle `40001 serialization_failure` with a bounded retry (the error is the feature, not a bug).
- **Never `SELECT` then `UPDATE` on a race row** without `FOR UPDATE`/`SKIP LOCKED` (or `UPDATE ... WHERE` with expectation checked via `RETURNING` / rowcount). "Two users both saw inventory 1, both bought" is the classic.
- Keep transactions **short**: no network I/O, no file writes, no "while" loops inside. A 2 s transaction holding locks = your whole app's latency ceiling.
- **Deadlocks**: consistent access order across code paths; Postgres detects and kills one (40P01) — retry with jitter, idempotently. Deadlock = bug, retry loop = the fix, not silence.
- Batch writes: chunk (1–5k rows), one txn per chunk, advisory locks for cross-request serialization (`pg_advisory_xact_lock`) when a "one winner" is needed.
- `SKIP LOCKED` = the queue pattern (claim work rows atomically) — implement queues in Postgres honestly before reaching for a broker.

## Migrations (the thing that breaks prod)

- **Forward-only, small, reviewable.** A migration that runs 30 s on 10M rows is not "small."
- Add column to a big table: (1) add nullable, (2) backfill in chunks (loop `UPDATE ... WHERE ctid ... LIMIT 1000` or set-based batches, sleep between), (3) set_not_null + default. (Modern PG11+ `ADD COLUMN ... DEFAULT` is lock-light but backfill-heavy — still chunk big ones.)
- **Never edit a shipped migration** (new migration to fix); migrations are an immutable log. Name with date + verb (`20260906_add_idx_orders_tenant_status.sql`).
- Migrations run **in CI against a copy of prod-data-shaped** (synthetic at prod scale — a migration that's fine on 1k rows locks for minutes at 10M); production deploy runs migrations *before* code (expand → migrate → contract pattern: new column nullable → code writes it → backfill → make required → drop old).
- Rollback = "the next migration" (fix-forward); `down` migrations for data-destructive steps only, tested.
- Index creation on big tables: `CREATE INDEX CONCURRENTLY` (no write lock; not in a txn).

## Scale & reads

- **Connection pooling** near the app (PgBouncer, transaction mode; keep app pools small: 10–50 per node). Databases don't scale with 5k connections per app node.
- **Read replica** for report-ish reads; accept replication lag (ms–s) or route critical-fresh reads to primary; application must tolerate stale (it's in the contract).
- **Partitioning** by time/range once a table crosses ~50–100 GB *and* queries prune partitions (time-bucketed access); before then, archive strategy (move old rows to cold table/Bucket) beats repartitioning drama.
- **Vacuum/bloat**: autovacuum tuned on hot tables (`autovacuum_vacuum_scale_factor 0.05` + fillfactor for update-heavy); bloat shows up as "indexes growing, queries slowing, disk filling" with no new data.
- **Backups = restore-tested or fake:** nightly physical (pg_basebackup/WAL archive for PITR) + regular WAL (point-in-time recovery to any second). **Run the restore quarterly** — an untested backup is a hope, not a backup.

## Caching (pairing with data)

- Cache-aside: read cache → on miss DB → set with TTL; write DB → invalidate (don't "update the cache" — invalidation is the sane default; TTL is the safety net for the invalidation you forgot).
- **Stampede** (expiry thundering herd): single-flight (one in-flight refresh per key) or jittered TTL (±10%).
- What to cache: expensive reads (aggregates, profiles, config); invalidation policy explicit per key. What not to: small fast primary reads (cache adds variance for no gain), anything with fresh semantics (balances, inventory) without a version.
- Hot rows (a trending item): local (L1, in-process, short TTL) + shared (L2); per-instance L1 absorbs the herd.

## Failure signatures → cause (first five checks)

"Query suddenly slow" → (1) recent data growth crossing a plan threshold (re-check EXPLAIN), (2) missing index added by a new pattern, (3) autovacuum off / bloat, (4) lock wait (someone is in a long txn), (5) replica lag reading as "data wrong."
"Deadlock storm at 9am" → batch job + user load sharing rows in different orders → fix order or separate windows.
"OOM in the DB" → one unbounded query (LIMIT / plan check) or connection pool runaway (bouncer).
"Migrations lock prod" → big table DDL without CONCURRENTLY / chunking, or `ALTER TABLE` validating a FK (heavy).
"Data wrong after restore" → PITR to before the WAL archive started (archive coverage gap) → check WAL retention, not the "restore."

## Checklist before a schema/DDL ships

[ ] FKs + ON DELETE semantics · [ ] indexes per hot query (composite order correct) · [ ] EXPLAIN on the top queries · [ ] money = numeric, time = timestamptz UTC · [ ] migration chunked for big tables · [ ] no long transactions in code path · [ ] backfill plan + backfill verification (counts) · [ ] restore tested within the last quarter.

## Detailed coverage

Data & database engineering — choosing store by workload, relational schema design, indexing & query analysis (EXPLAIN), transactions & concurrency (isolation, locking, deadlocks), migrations without locking, caching patterns, N+1 fixes, replication/partitioning/backup, and failure signatures. Postgres-centric; notes where other stores differ. Use when designing a schema, fixing a slow query, or choosing a data store.
