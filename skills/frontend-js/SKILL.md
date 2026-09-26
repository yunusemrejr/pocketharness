---
name: frontend-js
description: 'Engineer browser JavaScript/TypeScript: async behavior, event handling, workers, state, memory and DOM performance. Use for frontend logic and browser-runtime bugs.'
---


# Frontend JavaScript Engineering

For the compact implementation workflow start with `browser-javascript-engineering`; this guide provides deeper browser-runtime background. Use `typescript-contract-engineering` for type/module contracts and `node-runtime-engineering` for server runtime behavior.

## Event loop (the mental model that explains 80% of bugs)

- An event loop runs tasks and microtask checkpoints; browsers have multiple task sources and workers have separate event loops. A long task or recursively queued microtasks can delay rendering. Do not assume every timer, I/O callback and UI event shares one globally ordered queue.
- `await` = a microtask yield; `await a; await b;` in a loop = N yields (fine usually; in a hot loop of 1k awaits it's measurable — `Promise.all` when independent).
- **Zero-cost is a myth:** `setTimeout(f, 0)` is not "now" (clamped, next macrotask); microtasks inside a loop can starve paint (a `while` of Promise chains = frozen UI).
- Rendering: browser batches style/layout/paint after JS yields; **read-after-write interleaving** (`el.style.a=1; el.offsetHeight; el.style.b=2` → 3 reflows) — batch reads, then writes (or use `getComputedStyle` in one pass). `requestAnimationFrame` = write before paint.
- `requestIdleCallback` (gap-filling work), `queueMicrotask` — but never busy-wait (a spinning `while` kills the tab).

## Async correctness (the real bugs)

- Cancel superseded/abandoned work where the API supports it, and guard against stale results before updating state. Aborting alone does not undo already completed work or guarantee that every later continuation stops.
- `Promise.all` (first reject rejects) vs `allSettled` (await all, handle per-result) — pick deliberately; neither is "correct by default."
- Give each Promise chain an error owner. Browsers report unhandled rejections; process termination behavior in Node depends on runtime/settings. Do not equate every rejected browser promise with a process crash.
- Retry: only GET/idempotent, backoff+jitter, capped; 429 honors `Retry-After` (see `api-design`, `distributed-systems`).
- Choose bounded network deadlines appropriate to the operation, using supported AbortSignal/controller APIs. Distinguish timeout from user cancellation and reconcile uncertain mutations before retrying.
- Bound parallel requests according to server limits, payload size and measured latency. Avoid an unbounded Promise.all over large inputs; no universal concurrency number fits every service.

## Concurrency beyond the main thread

- Consider a worker for measured CPU work that blocks interaction, accounting for startup and transfer costs. Input size alone is not a reliable cutoff. Test responsiveness on target devices and move or chunk the actual bottleneck.
- `postMessage` **transfers** (`ArrayBuffer`) instead of cloning (`transfer:[buf]`) — zero-copy; for repeated RPC use **Comlink** (proxy semantics, structured ops) over hand-rolled message tables.
- `SharedArrayBuffer` + Atomics only for lock-heavy shared scratch (needs **COOP/COEP** site-wide — the same tradeoff as wasm threads; see `wasm-browsers`).
- Worker pools (2–4) when work is bursty; workers can spawn webview-level workers? No — they can spawn more workers (except dedicated→dedicated limits).
- OffscreenCanvas = Canvas rendering off-thread (heavy Canvas dashboards/charts).

## Memory (leaks are the "it gets slow over the day" bug)

- The four classic leaks: **detached DOM** (references kept after removal — `el` captured in a closure/timer), **un-cleared timers/listeners** (every `setInterval`/`addEventListener` has a paired, guaranteed cleanup — component unmount / page nav), **unbounded caches** (module-level `Map` that only grows → LRU with a cap, or WeakMap when keys are objects), **large globals** (a "temp" array that lives forever in a module scope).
- Give subscriptions one teardown owner. Pass signals only to APIs that support them; observers, timers and library subscriptions may require explicit disconnect/clear/unsubscribe.
- Closure captures: a timer callback referencing a 10 MB array keeps it alive for the timer's lifetime — capture the *index*/reference, not the data, or clear the capture on teardown.
- **`structuredClone` over JSON round-trip** (handles Dates, Maps, Uint8Array; JSON drops them / mangles types).
- Detect: Chrome Memory panel → heap snapshot **diff after N repeated actions** (open/close a modal 20×, navigate pages) — objects that shouldn't persist are the leak; `performance.measureUserAgentSpecificMemory()` for per-part attribution; long-term: watch RSS with a load script (a steady 10 MB/hour climb is a leak even if the tab "feels fine").

## State architecture (server state vs client state — #1 architecture decision)

- **Server state is not state: it's a cache.** Use a server-state manager (TanStack Query / SWR / Apollo) for API data: fetch, cache, dedupe (10 components need the user → 1 request), background refetch, mutation + **invalidation** (`queryClient.invalidateQueries({queryKey:['orders']})`), and it handles the stale-response races *for you*. Hand-rolled `useEffect` + `useState` + manual loading/error = re-implementing 40% of TanStack Query badly.
- **Client state** (UI selections, form drafts, feature flags): colocation first — `useState`/store for what a component subtree needs (Zustand/Redux only when many distant components + complex updates). Rule: **state lives as low as possible, as high as necessary.**
- Derived state is computed, not stored (a `totals` array that must match `rows` = a bug waiting; compute it, or make `rows` the only source).
- Forms: uncontrolled inputs + a single submit-time validation path (native `checkValidity`), or controlled from one source with one writer — never "sync both" (two sources = drift).
- Optimistic UI for likely-successful mutations (update cache, revert on failure with the server's error visible — `api-design` error model).
- SSR/hydration: **first HTML and first JS render must be identical** (deterministic: no `Date.now()`/`Math.random()`/user-only data in first paint without a strategy) — mismatches = hydration warnings + visible DOM flips.

## DOM & rendering

- Event **delegation** for dynamic lists (one listener on the container, `event.target.closest('[data-id]')`) — 10k listeners is a startup + memory tax.
- **`passive: true`** on scroll/touch listeners (enables smooth scroll; block-event-in-listener = console errors + jank); `touch-action` CSS for gesture control.
- `IntersectionObserver` (reveal, lazy-load, infinite-scroll sentinel) — never `scroll`-event math; `ResizeObserver` for responsive-without-resize events.
- `content-visibility: auto` + `contain-intrinsic-size` = free skip-render for offscreen sections (long lists, dashboards).
- Virtualize lists > ~100 visible-dense rows (or `content-visibility` first, virtualize when that's not enough).
- Form semantics: real `<form>`, `<label for>`, native validation + `details/summary` before reaching for a lib; **progressive enhancement**: core flow works with JS off (SEO + robustness + the 5% of users with an old device); `noscript` with honest content.
- Framework choice: the framework is a rendering/scheduling tool; **the bugs above (races, leaks, state) are framework-agnostic** — learn those, the framework is the syntax. Web Components only when embedding into hosts you don't control.

## TypeScript (as a bug filter, not a ceremony)

- `strict: true` + `noUncheckedIndexedAccess` + `exactOptionalPropertyTypes` — these three catch real classes of runtime bugs, not just "type noise."
- **Types at the boundary**: parse/validate external data (zod/valibot/io-ts) → typed domain model; `any` is banned, `unknown` is the boundary word (narrow with `in`/discriminated unions or a guard).
- Every `as` cast = a TODO: why? (a cast that hides a real shape mismatch is a silent bug; a cast with a one-line `// cast ok: X` is a contract).
- Exhaustive switches: `const _exhaustive: never = value` after the switch — add a case without handling it = compile error.
- Discriminated unions over boolean flags (`{kind:'draft', words:number} | {kind:'published', publishedAt:string}` — the type forces you to handle each state).
- `satisfies` for config objects (type-check + inferred literal types).

## Module & build ergonomics

- ESM everywhere; **dynamic `import()`** for routes, PDF/Excel/XLSX parsing, 3D, ML — never in the initial bundle (see `web-performance` budgets).
- No barrels (`index.ts` re-exporting 40 things) in library code — tree-shaking dies; import deep paths.
- `import.meta.env` for config; no runtime `require`/`eval`/`new Function` (CSP + perf + security — `web-security`).
- Version pin tooling (eslint/prettier/typescript exact); one formatter, one linter, zero debates.

## Testing strategy (what actually catches things)

- **Unit: pure logic** (parsing, validation, state reducers, formatting) — fast, deterministic, no DOM.
- **Component: Testing Library** (real DOM in jsdom — queries by role/label, not implementation; jsdom can't test layout/canvas/scroll — know its lies and cover those in E2E).
- **E2E: the 5–10 user journeys** (login, sign-up, checkout, edit-profile, the one flow that pays) in Playwright against a real stack (testcontainers + seeded data); mock only *external* 3rd parties (payment PSP). Not "a test per button."
- **Contract tests for API clients** (generated from OpenAPI — `openapi-typescript`/zod-generated) — the server lies and the client believes it, until the contract test.
- Snapshot/visual regression: sparingly (golden-path renders), perceptual diff, review diffs in PR; flaky visual tests get deleted, not retried-into-green.
- Test-speed budget: unit < 30 s, component < 3 min, E2E < 10 min — beyond that nobody runs them and they rot.

## Production-bug index (symptom → cause)

"UI freezes for a second then works" → main-thread > 50 ms work (parse/render loop) → worker or chunk. "Tab gets slower all day" → leak quad (see Memory). "Wrong data shows briefly, then corrects" → stale-response race / no invalidation → AbortController + server-state manager. "Console: 'cannot read prop of undefined' on refresh, not on dev" → SSR mismatch / race on first paint. "Works in dev, 403s in prod / or white of CSP errors" → inline styles/scripts (CSP), mixed-origin service worker scope, cookie domain. "Animations jank on scroll" → non-passive listeners / scroll-event math / layout-affecting animation (see `motion`). "Memory panel: hundreds of 'el'" → detached DOM (un-cleared listeners/timers).

## Detailed coverage

Frontend JavaScript/TypeScript engineering — event-loop & microtask model, async/await semantics, workers & concurrency off the main thread, memory-leak patterns & detection, DOM/rendering performance (throttle, containment, virtualization), API-client correctness (caching, race conditions), state architecture (server vs client state), testing strategy, and the recurring production bugs with their causes. Use when writing or reviewing JS/TS frontend code, debugging UI performance or "works but wrong" behavior.
