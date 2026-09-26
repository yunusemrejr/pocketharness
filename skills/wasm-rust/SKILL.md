---
name: wasm-rust
description: Build Rust WebAssembly with target selection, wasm-bindgen interop, packaging, size optimization and debugging. Use for Rust-to-browser integration and WASM runtime issues.
---


# Rust → WebAssembly

## Targets (pick the right one first — it changes everything)

| Target | What it is | When |
|---|---|---|
| `wasm32-unknown-unknown` | browser target, JS interop via **wasm-bindgen**, no POSIX (partial std: no fs/threads without extra features) | **Default for the browser** (library or webview app) |
| `wasm32-wasi` | WASI (POSIX-ish: files, clock, env), **no JS** | standalone binary (wasmtime/wasmedge), services, tests |
| `wasm32-unknown-emscripten` | Emscripten toolchain (fs, DOM via syscalls, bigger runtime) | when you need Emscripten's fs/thread model or C interop at scale (see `wasm-c-cpp`) |

Add: `rustup target add wasm32-unknown-unknown`. Pure-logic crates build **natively too** → test both sides in CI (native unit + wasm integration) — the native port is your fast dev loop.

## wasm-bindgen: the interop surface

```rust
#[wasm_bindgen]
pub fn process(input: &str) -> Result<Vec<u8>, JsError> {
    parse(input).map_err(|e| JsError::new(&e.to_string()))  // across the boundary = JS exception
}
```

- **Strings**: `String ↔ &str / JsValue` are bound; but per-call conversion is allocation — hot paths keep the JS handle, or better, **typed arrays** (below).
- **Typed arrays (the zero-copy path)**: the *automatic* `Vec<u8> → Uint8Array` binding **copies**. For real size/speed: expose the linear memory slice or return owned buffers explicitly:

```rust
#[wasm_bindgen]
pub fn to_base64_slice(src: &js_sys::Uint8Array, dst: &js_sys::Uint8Array) {
    // read src directly (it is wasm memory), write dst directly — no copies
}
```
  Pattern: JS allocates the output `Uint8Array` (or you hand back a fresh one **with a `drop()` companion** — `#[wasm_bindgen] pub fn drop(ptr: *mut u8, len: u32)`), or use a view into `WebAssembly.Memory.buffer()` (`js_sys::Uint8Array::new(memory.buffer()).subarray(off, off+n)`). **Ownership rule: exactly one side frees; say which in the doc comment.**
- **Async**: JS Promises are first-class — `wasm_bindgen_futures::JsFuture::from(js_promise).await`; async `#[wasm_bindgen] fn`s return JS Promises (compile needs the `serde`-less `web-sys` glue; just enable).
- **Errors**: panic **aborts the wasm run** (process dies, no "catch") — map expected failures to `Result<T, JsError>`/`anyhow + #[wasm_bindgen(js_name=...)]` and let the JS side `.catch()`. `unwrap()` in browser-facing code = a ship-time crash the user can't debug.
- **Closures/callbacks**: `Closure` (clone for repeated use, `forget()` for live-forever — and remember to `drop` in JS teardown), `Closure::once` for one-shot.
- DOM access via `web-sys` (typed) + `gloo` (convenience: events, timeout, channel, workers); `js-sys` for JS APIs without web-sys.

## Build, size, performance

- **wasm-pack**: `wasm-pack build --target web` (browsers, ES modules) / `--target bundler` (webpack/rollup) / `--target nodejs` (test harness). It runs `wasm-opt` (`--optimize`/`--release` pass `--optimization-level 3`).
- **`[profile.release]`** for wasm: `lto = "fat"`, `codegen-units = 1`, `panic = "abort"` (no unwinding, small, no table growth), `opt-level = "s"` or `"z"` when size matters (usually `"z"` for a library), `strip = true`. Expect **−30–50% size vs defaults** and +10–20% speed from LTO.
- `#![no_std]` + `extern crate alloc` for logic-only crates: std overhead (~150–300 KB) disappears; `wee-alloc` is mostly obsolete (default GlobalAlloc from `alloc` is fine).
- **simd128**: `-Ctarget-feature=+simd128` (or `target_feature = "simd128"` cfg + `std::simd`/`wasm_simd` crate gates) — **also ship the non-SIMD build and dispatch in JS** (see `wasm-browsers`): two artifacts, one entry.
- Bench before and after (criterion native + wasm-bindgen-test in browser) — "Rust is fast" is true and useless until you've measured *your* codepath (often the boundary, not the compute: see above).
- Deps cost is real in wasm: every crate's code is in the binary (LTO helps). Audit: `wasm-pack build` + `wasm-opt -S` or `wasm-objdump`; a `serde`+`json`+`regex` stack adds hundreds of KB — for a browser tool, prefer `rkyv`/`postcard` over JSON for internal formats.

## Whole app in Rust (Yew/Leptos/Dioxus)

- **Leptos** (signals, SSR + hydration, Rust-first — my default for new Rust web apps), **Yew** (hooks, React-like, most mature ecosystem), **Dioxus** (multi-target incl. desktop/native, Tauri pairing). **Trunk** = build tool (HTML entry, inline `rust!`, hot reload, asset hashing).
- Keep the UX honest: the web's ecosystem (design systems, a11y libs, charting) is *vastly more mature in JS*; Rust-WASM wins where **logic density** lives (parsers, engines, editors, heavy compute + a modest UI). A Leptos marketing site is ponytail-violation: the UI is JS's home turf (see `anti-ai-slop` — ship the right tool).
- SSR + hydration (Leptos/Dioxus isomorphic): deterministic first render (no `Instant::now()` in markup), hydration mismatch = the same bugs as React's (see `frontend-js`).

## Threads (wasm-threads) in the browser

- Enable in the crate: `features = ["rayon"]` (rayon is the sane path; `std::thread` needs `-Ctarget-feature=atomics.wait` + shared memory) — **requires COOP/COEP site-wide** (`window.crossOriginIsolated === true`) or instantiation **fails** (see `wasm-browsers` for the header matrix + lockout risks).
- Spawn model: the main thread runs wasm-rayon, `spawn_worker()`-style creates web workers running copies of the module; data crossing = typed-array transfers (ownership moves).
- Most browser Rust code is *single-threaded + async* (I/O bound) — threads only when you have embarrassingly-parallel compute (batch transforms, encoding) **and** the deployment can guarantee the headers.

## Testing (in real browsers — not Chrome-only "node")

- `wasm-bindgen-test`: `[[test]] crate-type = ["cdylib","rlib"]`; `wasm-pack test --headless --chrome` (and `--firefox`); these run the **real JS glue + real GC semantics** (node-based runs miss DOM/JS interop bugs).
- CI matrix: **chromium + firefox + webkit** (webkit headless covers Safari-class behavior reasonably; real Safari on a macOS runner for the final gate — see `wasm-browsers`).
- Fuzz the *native* port (cargo-fuzz) + a few browser matrix runs — wasm-specific bugs (boundary memory, GC timing) show up in the bindgen tests.

## Deploy & ship

- Static asset: hash the `.js`/`.wasm`, `Cache-Control: immutable`, **streaming compile** when supported (needs `Content-Type: application/wasm`; Safari historically doesn't true-stream — fallback path is the default) (see `wasm-browsers`).
- Version the **pair** (js + wasm) or they disagree (the glue's import names must match the module — never mix versions); pin the toolchain (`rust-toolchain.toml`) so a recompile is byte-identical-ish (reproducible builds: same toolchain + flags ≈ same module, still hash-verify in CI).
- Size budget: pure-logic library **< 200 KB** (no_std, LTO, no json/regex); a small Leptos app **< 500 KB** is reasonable; > 1 MB = audit deps and consider a JS-front + Rust-core split.

## Detailed coverage

Compiling Rust to WebAssembly — toolchain targets (wasm32-unknown-unknown vs -wasi vs emscripten), wasm-bindgen interop (strings, typed arrays, async, Result→JS exceptions), wasm-pack/binaryen size & perf (LTO, panic=abort, no_std), frontends (Yew/Leptos/Dioxus), WebAssembly threads in Rust, testing in real browsers, and deployment. Use when building a Rust→wasm library, a Rust web app, or porting Rust code to the browser.
