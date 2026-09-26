# Rust Systems Engineering: patterns and examples

## Ownership and errors
Return borrowed views only when the owner's lifetime is clear. Prefer owned values across independently scheduled tasks. `Arc<T>` shares ownership, not mutation safety; a mutex around everything can serialize the service and hide a bad state model. Avoid holding a synchronous lock across `.await`. Check whether cancellation after a write but before acknowledgment requires idempotency.

```rust
fn read_count(text: &str) -> Result<usize, std::num::ParseIntError> {
    text.trim().parse()
}
```
Parsing a number is not validating a business limit: enforce maximum allocation/count separately. At application boundaries attach useful context without exposing secrets; use typed errors when callers need to branch. Panics represent violated assumptions, not routine input rejection.

## Unsafe and FFI
For each unsafe block specify pointer validity, alignment, initialization, aliasing, lifetime and thread assumptions. `repr(C)` provides a layout contract, not ownership or lifetime management. Define who allocates and frees buffers, what zero length permits and how errors cross the boundary; do not unwind through incompatible foreign frames. A safe wrapper must reject invalid lengths and keep backing allocations alive. Pinning is not a general ownership repair.

## Runtime and delivery
Choose async for concurrent waiting, not automatically for CPU work. Move heavy work to bounded workers; keep cancellation, deadlines and shutdown explicit. Check Send/Sync requirements against the actual executor. A channel needs a capacity and a slow-consumer policy. Verify host/target dependencies for cross compilation; a musl target alone does not prove every native dependency is portable.

Run formatting, linting and contract tests with the repository's toolchain. Use Miri where supported for unsafe-focused tests, plus fuzzing and sanitizers as applicable; passing any one tool is not proof of soundness. Benchmark release builds including allocations and realistic input distributions before adding custom allocators or unsafe SIMD.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://doc.rust-lang.org/book/
- https://doc.rust-lang.org/nomicon/
- https://rust-lang.github.io/unsafe-code-guidelines/
