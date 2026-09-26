---
name: memory-resource-ownership
description: Design lifetimes and cleanup for buffers, handles, subscriptions, foreign-function interfaces and asynchronous resources; use for leaks, use-after-free or ownership ambiguity.
---

# Memory Resource Ownership

List resources and their acquire, transfer, borrow and release points. Include files, sockets, timers, callbacks and GPU allocations, not only heap memory.

1. Assign one authoritative owner for release. Model borrowed references and shared ownership explicitly; avoid retaining a resource merely because a callback can outlive its creator.
2. Follow every exit path: success, partial initialization, exception, cancellation and shutdown. Prefer the language's established scope-based cleanup mechanism.
3. At FFI boundaries verify ABI, layout, alignment, encoding, allocation/free pairing and whether foreign code retains pointers. A matching type name does not establish compatible layout or lifetime.
4. Test repeated acquire/use/release and failure at each acquisition boundary. Use sanitizers, leak tools or allocation tracking supported by the actual runtime; garbage collection does not clean up all external resources.
5. Check that cleanup is idempotent where multiple paths may invoke it and that late callbacks cannot access released state. Do not solve a race by leaking the resource indefinitely.

Example: returning a view into a temporary buffer can appear correct until the allocator reuses its storage. Copy, transfer ownership or extend the owner's lifetime deliberately.

Deliver the ownership contract and failure-path checks. Keep changes at the existing lifecycle owner rather than adding a parallel global cleanup manager.
