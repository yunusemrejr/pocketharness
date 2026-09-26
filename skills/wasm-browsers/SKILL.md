---
name: wasm-browsers
description: Check browser WebAssembly support, feature detection, isolation headers and fallbacks. Use for cross-browser WASM failures or deployment compatibility.
---


# WebAssembly: Browser Compatibility

## The mental model

Wasm is a **graduated spec**: every new feature lands in a staggered order (Chrome → Firefox → Safari, ~6–18 months apart) and **never** in legacy engines (IE never had Wasm; legacy Edge 15–18 — still on some EOL enterprise boxes — never did either). So the product question is always: **what is my feature floor, and what do I do below it?** Answer with runtime probes, never UA sniffing (proxy-rewritten UAs, webviews, and "Chrome" that isn't Chrome will lie to you).

The live authority for versions: **caniuse.com/wasm** (check it before committing to a floor; this table is the shape of the answer, not the current numbers).

## Feature matrix (shape, verify exact versions on caniuse)

| Feature | Chrome | Firefox | Safari | Notes |
|---|---|---|---|---|
| **Baseline Wasm** (MVP, 32-bit memory) | 57+ | 52+ | 11.1+ | Everything post-~2019 evergreen. Legacy Edge/IE: **none** → JS fallback required for those envs, full stop |
| **Streaming compilation** | 66+ | 60+ | 14.1+ | `instantiateStreaming` — needs `Content-Type: application/wasm` (+ CORS); **Safari historically compiles after full download**, not true early-stream — fine, but don't rely on "start compiling before download done" |
| **Bulk memory ops** | 91+ | 94+ | 15+ | Mostly toolchain-transparent (emscripten/rust emit it when target supports); floor ~2021 |
| **SIMD (simd128)** | 91+ | 89+ | 14+ | 2021+. Build with/without → dispatch (below). Rust `-Ctarget-feature=+simd128`, C `-msimd128`/intrinsics, binaryen `-mfeatures=+simd128` |
| **Exception handling** | 122+ | 121+ | 17+ | 2024. **Don't target yet** — toolchains (Emscripten longjmp, Rust panic=abort) work around it; new EH-based code has no floor story |
| **Reference types / externref** | 84+ | 89+ | 14+ | JS↔wasm references, GC-friendly; Rust `externref` feature (stabilizing era) |
| **Tail calls** | 123+ | 135+ | 17.4+ | 2024. Avoid deep-recursion patterns that *need* it; most code doesn't |
| **Threads (+ SharedArrayBuffer)** | 91+ | 111+ | 15.4+ | **The big one** — requires the cross-origin-isolation header pair (below). Everything before F111/S15.4 = single-threaded |
| **WasmGC** (JS-typed objects, `externref`-rich) | 115+ | 130+ | 18.4+ | All-evergreen ~2025; toolchains (Rust `wasm32-wasip1` GC? no — Rust's GC tier is newer; Emscripten `-fexperimental-wasm-gc`) — treat as "new, opt-in, not default" |
| **memory64** | 111+ | 127+ | 18.2+ | >4 GB linear memory; browser apps rarely need it — if you hit the 2/4 GB wall, re-arch (shard buffers) before targeting memory64 (floor is 2024+) |
| **Relaxed SIMD** | 127+ | 129+ | 18.2+ | 2025. Do not target (no floor story, niche gain) |

**Practical floors (2026):** baseline ≈ everything evergreen; SIMD ≈ Chrome 91/FF 89/Safari 14 (safe default build target *with* a non-SIMD fallback); threads ≈ 2023+ evergreen **and** you can guarantee headers; GC/EH/memory64 ≈ "newer 2024–2025" — don't make product dependencies on them yet.

## Detection: probe, don't sniff

```js
// 1KB probe per feature; WebAssembly.validate exists everywhere Wasm exists
const SIMD = new Uint8Array([0,97,115,109,1,0,0,0, 1,4,1,96,0,0, 3,2,1,0, 10,8,1,6,0,65,0,0,11]); // your real probe bytes
const hasSimd = WebAssembly.validate(SIMD_PROBE);      // synchronous, no instantiation cost
const crossOriginIsolated = window.crossOriginIsolated; // the threads gate (headers, below)
const canStream = 'instantiateStreaming' in WebAssembly && navigator.gpu?.requestAdapter ? true : 'instantiateStreaming' in WebAssembly;
// dispatch: full-simd module → non-simd module → pure-JS implementation (see Build strategy)
```

- `WebAssembly.validate()` is the clean runtime gate (it accepts feature-gated bytes and reports acceptability **without instantiating**) — keep 2–4 tiny probe modules in a versioned file; test them in your browser matrix (a probe that's wrong is a silent wrong-dispatch).
- `crossOriginIsolated === true` is the **only** honest threads check (it *is* the header result).
- UA sniffing is forbidden in this file and in your codebase. Ever.

## Threads = the cross-origin-isolation lockout (read before enabling)

Wasm threads (and `Atomics`, `SharedArrayBuffer`) require the site to be **cross-origin isolated** — two response headers on **every response** (HTML, JS, workers, fonts, images, API, CDN assets even):

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp     # or: credentialless  (see below)
```

Consequences — this is where sites brick themselves:

- **`require-corp`** demands that *every cross-origin subresource* carry `Cross-Origin-Resource-Policy: same-origin` (or same-site) — one un-CORP'ed CDN asset = that asset 403s and (worse for some) the whole origin is no longer cross-origin-isolated → threads silently unavailable. Audit: `curl -sI <asset> | grep -i cross-origin` across the whole asset graph (CDN config, not just your server).
- **`credentialless`** is the looser COEP: cross-origin frames/assets load **without cookies/credentials** — usually the right choice for marketing-ish sites that embed third-party content; iframes lose their login state (a logged-in embed becomes guest — check your embedded content survives).
- **What else dies under COOP/COEP:** `crossOrigin`-sharing between windows (postMessage still works; `window.opener` access is restricted — you can `window.open` in a new window; you can't *read* other same-origin windows' content across the opener chain), some analytics/SDKs that use `window.opener`, and any "shared window" UX. Test the full site under the headers **before shipping** — not in dev (dev often serves without the headers and lies to you).
- **Partial rollout**: headers can't be per-route — the whole origin (same eTLD+1) is committed. A subdomain that *isn't* isolated can't host the threaded module.
- **Decision rule:** threads only when (a) the compute truly needs it (measured: single-threaded doesn't meet the latency budget), (b) you own/audit the full asset graph, (c) you can accept the embed/login tradeoffs. Otherwise: chunked async, Web Workers (workers don't need COOP/COEP — only *shared* memory does), or a smaller algorithm.

## Per-browser quirks (the parts that actually hurt)

- **Safari/iOS:** no true streaming (compile-after-download — budget time, not bandwidth); aggressive **low-memory kills** on iOS (a tab with > ~1 GB of wasm+heap in background = killed; size the heap down, release on `visibilitychange`→hidden for huge buffers); worker+wasm works but test **real** Safari (webkit headless in CI ≈ but ≠); `WebAssembly.Memory.grow` works but grow in **64 MB steps** (frequent small grows = fragmentation).
- **Firefox:** oldest-threads browser of the three (111) — your thread floor; `wasm-opt -O4` (MARS unrolling) can be slower in FF than Chrome for tight numeric loops — keep `-O3` for cross-browser uniformity unless Chrome-only is the contract.
- **Chrome/Android + WebView:** desktop Chrome = leading edge; **Android WebView lags by device update cadence** (old devices run 1–2 years behind Chrome — Plan for the *device fleet*, not "Chrome"); Chrome desktop is your *leading* edge, not your guaranteed floor.
- **Legacy (IE11/Edge 18):** no Wasm at all → the pure-JS fallback isn't a nice-to-have, it's the feature (see Build strategy); don't ship `WebAssembly.js` polyfill nonsense — a real JS implementation is the fallback (often your "JS tier" is exactly that).
- **Embedded webviews** (Electron, Tauri, desktop apps, mobile app in-app browsers): the engine = the host's Chromium/WebKit version — **older than the user's browser** (and you can't force-update). Electron: pin a Chrome-you-control (good leverage — you *set* the floor); in-app webviews on old Android = the slow pole. Test in your actual shell, not just the system browser.

## Build & dispatch strategy (the artifact you ship)

1. **Tier 0 (JS)**: a correct pure-JS implementation of the core path. Not an afterthought — it's the legacy/old-webview/first-paint fallback and your reference for correctness.
2. **Tier 1 (wasm baseline)**: the core, no SIMD — works on everything evergreen since ~2019.
3. **Tier 2 (wasm + SIMD)**: the fast path (2–8× on numeric code).
4. (Optional) **Tier 3 (threads)**: only if measured, and only under verified `crossOriginIsolated` — never a hard requirement; dispatch off.

```js
// dispatch: probe → load the highest tier the host actually supports
// (hash-versioned static assets, streaming where supported, bytes fallback otherwise)
```

- **One toolchain, multiple outputs**: binaryen `-mfeatures` / C target-feature / Rust cfg produce the tiers from the same source; CI builds + unit-tests each tier **in each browser** (the "works in Chrome" tier gate is the whole point).
- **Never mix versions of the js-glue and the .wasm** (import mismatch = silent `Linking failed`); version them as one unit, hash-verify in CI.
- **MIME**: `Content-Type: application/wasm` on every `.wasm` (Safari is strict; wrong/missing MIME = `instantiateStreaming` rejects → you must have the bytes fallback anyway).
- Cache `immutable` (hashed names); the wasm bytes are immutable per version.

## CI matrix (non-negotiable minimum)

- **Chromium + Firefox + WebKit (headless)** via Playwright on every PR that touches the wasm build or dispatch; **real Safari on `macos-latest`** as the release gate (Playwright's webkit ≈ WKWebView, not Safari — close, not the same; the release gate is the real thing).
- **Android**: one mid-tier real device (or BrowserStack) for the thread/SIMD/memory tests monthly; the CI proxies can't see iOS/Safari at all — the macOS runner is the only path.
- Load test under headers: a nightly job runs the full site **with COOP/COEP on** and asserts `crossOriginIsolated === true` on the key routes + that the threaded path instantiates — a regression here (someone's new CDN asset breaks CORP) is otherwise a silent multi-day outage of the fast path.

## The honest summary

Baseline Wasm is a **2019 solved problem** — ship it, cache it, don't fear it. SIMD is a **2021 solved problem** — build both, dispatch on a probe. **Threads are a site-wide architectural decision** (headers, embeds, workers), not a build flag. GC/EH/memory64/tail-calls are **new** — consume, don't depend. Legacy is **JS**, always.

## Detailed coverage

Cross-browser WebAssembly compatibility — the feature matrix (baseline, SIMD, threads/COOP-COEP, bulk memory, GC, tail calls, memory64) by evergreen browser, runtime feature detection (WebAssembly.validate probes, NOT UA sniffing), streaming compilation & MIME, the cross-origin-isolation header lockout and how to not brick your site, per-browser quirks (Safari, iOS, legacy Edge, Android WebView), build-time strategy (multiple artifacts, simd/js fallback), and a CI test matrix. Use when a wasm feature fails in one browser, when shipping threads, or when choosing the feature floor for a product.
