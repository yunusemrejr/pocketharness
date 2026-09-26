# SQL Query and Transaction Engineering: patterns and examples

## Grain and joins
```sql
WITH totals AS (
  SELECT customer_id, SUM(amount) AS total
  FROM orders
  WHERE status = :status
  GROUP BY customer_id
)
SELECT c.id, COALESCE(t.total, 0) AS total
FROM customers AS c
LEFT JOIN totals AS t ON t.customer_id = c.id;
```
Parameter notation is driver-specific. Preaggregation avoids multiplying amounts when joining another one-to-many relation. Confirm whether missing totals mean zero, unknown or excluded. A filter on the nullable side in WHERE can turn a left join effectively into an inner join. `NOT IN` with NULL has surprising three-valued semantics; use a carefully correlated NOT EXISTS when that matches intent.

## Determinism and engines
For latest-row logic use a window function with an explicit secondary unique key for ties. Ordering inside a CTE does not guarantee final output order. Keyset pagination needs a stable composite order and matching comparison, including null policy. Timestamp with/without timezone, collation, case folding, JSON behavior, boolean syntax, upsert and RETURNING differ by engine/version. SQLite's affinity and concurrency differ from server databases; do not treat a SQLite unit test as proof of PostgreSQL/MySQL behavior.

## Transactions and performance
State the invariant under concurrent writers, then choose constraints, locks or isolation that enforce it. Read-modify-write can lose updates; atomic updates or version predicates can make conflicts observable. Retrying serialization errors must replay the whole transaction with bounded retries and safe side effects. DDL locking and transactionality vary; stage migrations, inspect lock duration and keep recovery possible.

EXPLAIN ANALYZE may execute the statement; use a safe representative environment for mutations. Indexes must match predicates/order and real selectivity, while accounting for write cost. Test empty sets, duplicates, nulls, skew, ties, large values and simultaneous transactions. Compare row counts and totals to an independent small fixture before trusting a faster plan.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.postgresql.org/docs/current/
- https://dev.mysql.com/doc/
- https://sqlite.org/docs.html
