# Java Platform Engineering: patterns and examples

## Data and resources
Use immutable value objects where practical, but remember a record containing a mutable list is not deeply immutable. Validate boundary inputs and preserve useful typed errors. Use try-with-resources for closeable resources; do not return streams whose database connection has already closed. Prefer parameterized SQL and explicit transaction boundaries over concatenation.

```java
static BigDecimal total(BigDecimal unit, int count) {
    if (count < 0) throw new IllegalArgumentException("negative count");
    return unit.multiply(BigDecimal.valueOf(count));
}
```
Construct decimal amounts from decimal strings or exact units; `new BigDecimal(double)` preserves binary approximation. Choose currency scale and rounding at the business boundary, not after every intermediate operation.

## Concurrency and persistence
An executor needs admission control and a shutdown policy. Virtual threads, where supported, improve some blocking workloads; they do not make CPU work faster or remove database connection limits. Verify current JDK behavior around blocking/pinning instead of repeating historical rules. Avoid propagating request state through unmanaged thread locals. Structured concurrency APIs may be preview/version-specific; never silently require preview features in a stable project.

ORM convenience does not remove N+1 queries, isolation anomalies or lazy-loading lifecycle. Check generated queries and fetch plans on realistic cardinalities. Retries need a bounded policy and idempotency; do not retry arbitrary partially committed transactions.

## Linux and numerical work
Container memory includes heap, metaspace, direct buffers, thread stacks and native allocations. Tune with measured pressure and latency; a heap limit is not total process memory. Check charset, locale and timezone dependencies. Use stable reductions and independent baselines for statistics; JNI/Panama/vector API availability depends on JDK and deployment. Benchmark with a harness that accounts for warmup and dead-code elimination, then validate service-level latency and GC behavior. Test the packaged jar/image and supported JDK, not only IDE execution.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://dev.java/learn/
- https://docs.oracle.com/en/java/
- https://openjdk.org/projects/
