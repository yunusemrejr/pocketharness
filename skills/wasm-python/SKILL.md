---
name: wasm-python
description: >-
  Running & shipping Python in the browser via Pyodide — loadPyodide/indexURL (the #1 bug), packages & micropip, PyProxy leaks (the #1 memory bug), JS↔Python data crossing (zero-copy via FS/Buffer, toJs/toPy), performance reality (CPython-in-wasm, NumPy via OpenBLAS, no GIL tricks), custom Python wheels via pyodide-build, JupyterLite, sandboxing user code, and when to use it vs ONNX/TF.js vs a server. Use when adding Python to a web app, porting a Python tool to the browser, or debugging Pyodide performance/memory.
---

# Python in the Browser (WASM)

## The two honest options

1. **Pyodide** — CPython itself compiled to WebAssembly (Emscripten). Arbitrary Python runs in the tab; no server. This is the skill's main path.
2. **"Python as a library you compiled"** — there's no such thing: Python has no wasm ABI. If the *deliverable* is "our Python service in the browser," the real options: Pyodide (it *is* Python), **JupyterLite** (Pyodide + Jupyter, static, docs/demos), or reframe: the browser UI talks to your **server** (Python stays on the server — most "run my Python in the browser" requests are actually "show my results without a server" → Pyodide or a precomputed bundle).

- Inverse (Python *host* running wasm): PyWasm/Wasmtime embedding — run Rust/C-compiled modules inside a Python backend (fast parsers, Emscripten tools) — that's a backend story (`distributed-systems` for the serving pattern), not a browser story.

## Pyodide basics (and the #1 bug)

```html
<script src="https://cdn.jsdelivr.net/pyodide/v0.27.x/full/pyodide.js"></script>
<script type="module">
const { loadPyodide } = await import("https://cdn.jsdelivr.net/pyodide/v0.27.x/full/pyodide.mjs");
const pyodide = await loadPyodide({ indexURL: "https://cdn.jsdelivr.net/pyodide/v0.27.x/full/" });
// ⚠ indexURL = the directory of pyodide.js itself. Relative on a subpath = 404 hell. Absolute. Pinned.
await pyodide.loadPackage(["numpy"]);
pyodide.runPython(`print(np.array([1,2,3]).sum())`);
</script>
```

- **The #1 Pyodide bug is `indexURL`**: it must point at the package directory (where `pyodide_*.js`, `repodata.json` live), **absolute**, and version-pinned. A relative indexURL breaks on any route other than `/`. Self-host (download once, pin the version, serve from your CDN) for offline/air-gapped/privacy products — the whole stack is static and immutable (cache `immutable`).
- **Boot cost**: core ≈ 10 MB (wasm + js) + stdlib. Budget a **progress UI** (loading bar); lazy `loadPackage` per feature (numpy ≈ 20 MB, pandas ≈ 30 MB, **scikit-learn ≈ 100+ MB — rarely fits a casual user**), stream + cache. Mobile first-load: aim total < 30 MB.
- **Packages**: `micropip` for anything not in the std-index: `await pyodide.loadPackage("micropip"); await pyodide.runPythonAsync("await micropip.install('scipy')")` — **pin versions** (micropip resolves latest by default; a "latest" that changes under your users is a non-reproducible product).
- **Your own Python code** ships as a static file (or `pyodide.FS.writeFile('/app/mylib.py', ...)`) — hash it, version it. `pyimport('mylib')` after install. No server, no build step — but **reproducibility is on you** (exact file + exact Pyodide version + pinned packages, recorded).

## JavaScript ↔ Python (the FFI that bites)

- **PyProxy** (Python object handed to JS) is a **live reference into the wasm heap** — **calling `gc`/leaking proxies = the #1 Pyodide memory bug.** Rule: **every `.toJs()`, every PyProxy you keep, gets `.destroy()` in a finally** — or wrap in a helper that destroys on GC-able WeakRef (still, explicit `destroy()` — don't trust finalizers for correctness).
- **Copying vs zero-copy:**
  - `proxy.toJs()`: copies (deep for lists/dicts — `toJs({dict_converter})`); a 50 MB DataFrame through `toJs` = allocation storm + slow.
  - **The zero-copy idiom**: write the result to **Pyodide's virtual FS** (`np.save('/tmp/out.npy', arr)` / `json.dump` to a path), then read via `pyodide.FS.readFile('/tmp/out.npy')` → `ArrayBuffer` → JS `typed array` (one copy, binary, fast). Same for images (`PIL → /tmp/out.png`). For repeated passes, keep the array **in Python** and call into it; cross the boundary only at the edges.
  - JS → Python the same way: `pyodide.FS.writeFile('/tmp/in.npy', arrayBuffer)` → `np.fromfile` / `np.load`. (Or a `PyBuffer` view for small, ephemeral data.)
- **JS objects to Python**: `pyodide.js` (`let np = pyodide.pyimport('numpy')`; `np.from_js([1,2,3])` exists for small data — for bulk, the FS path again).
- **Async**: `runPythonAsync` for code that awaits (micropip, fetch via `pyodide.http`); never block the main thread with a long `runPython` — the browser's 5-second "unresponsive" dialog is a product bug.
- **Streaming results** (a long computation): Python `yield` + a `PyIterProtocol` proxy → `for await` in JS, or a progress callback (`pyodide.setInterrupt`) + a JS polling loop — pick per UX; both need a **cancel path** (`pyodide.setInterrupt` = cooperative; heavy C-extension code can't be interrupted mid-C — bound the unit of work instead).

## Performance reality (set expectations early)

- **Pure Python in wasm: ≈ 3–10× slower than native CPython** (wasm interpreter overhead). A 2 s native script is a 10–20 s browser script — acceptable for user-initiated, unacceptably slow for page-load work.
- **NumPy/SciPy**: the heavy math runs in **C (OpenBLAS, compiled to wasm)** → near-native (SIMD is on in the Pyodide builds; no threads — a 300×300 matmul is ~fine, a 30k×30k one is a long day, or a server).
- **No threads** (GIL + single-threaded wasm in practice; Pyodide does not run worker-Pythons): heavy work = (a) chunk it with progress + cancel, (b) drop to C-level speed (NumPy/Cython/C extensions), (c) a web worker runs a **second Pyodide** (a separate interpreter — process the message, one Python per worker; no shared state, no GIL sharing — this is your "parallelism" and it costs a full interpreter each: ~10–20 MB + boot per worker), (d) move to the right tool (`browser-ml`: ONNX/TF.js for inference, `wasm-rust`/`wasm-c-cpp` for a compute core).
- **Mobile**: iOS Safari wasm is capped in effective memory (plan < 1–2 GB total app memory); a 2 GB numpy array is a desktop-only feature — detect and degrade.
- **CPU**: wasm on a mid-tier phone ≈ 5–10× slower than desktop-class for the *same* code — profile on a 2019-class device, not your M3.

## Custom Python packages (shipping your own wheel)

- `pyodide-build` (or the `@pyodide` tooling): a wheel with C extensions cross-compiles via Emscripten automatically for known packages; **most pure-Python packages work as-is** (`micropip`-installable from your own index or a local file): `micropip.install('file:///app/ourpkg-1.2.3-py3-none-any.whl')`.
- Build loop: `pyodide build ourpkg` → test **in a headless browser** (CI: load real Chrome, `loadPackage`, import, run tests) — node-based Pyodide tests exist but miss browser glue (timers, DOM, fetch semantics); the browser is the contract.
- Version your wheel with your Pyodide version both pinned in the app manifest (`{pyodide: "0.27.x", packages: {ourpkg: "1.2.3"}}`), and record exact fetch URLs — reproducibility is the whole product, here.

## JupyterLite (Python *notebooks* in the browser, no server)

- Static build of Pyodide + JupyterLab: ship `.ipynb` + a `lite` folder; users get a full notebook client offline (docs, demos, teaching, sandboxed demos).
- Budget the kernel (core + packages users actually run); extensions that need the server (terminal, files beyond the FS) are out; custom kernels = custom Pyodide build (same tooling as above).
- This is the highest-leverage "Python in browser" artifact for a *product that already has notebooks* — don't build a notebook UI if JupyterLite covers it (see `ponytail`).

## Sandboxing user code (security)

- Pyodide is **already a sandbox from the network/OS** (no fs, no sockets unless you expose them) — the risk is what *you* hand it:
  - **Don't expose `js` (DOM) or `fetch`** to user code by default; expose a minimal, typed facade (a `postMessage`-style API of named functions) — never `window`, never `eval` a string.
  - **Egress**: any user-supplied URL fetch goes through a server proxy with an allowlist (see `web-security` SSRF section) — in-browser, a user with `fetch` has your origin's cookies.
  - **CPU/DoS**: an infinite loop hangs the tab (or the worker). Put user code **in a worker** (dying costs the worker, not the page), bound wall time (worker timeout → terminate → reload), bound memory (wasm heap cap is soft; size-limit inputs: "max 100 MB data").
  - **Persistence**: Pyodide's FS is in-memory (reset per load). Persist explicitly (read → IndexedDB; never "the disk" — there is no disk).
  - **Separate origin for hostile user code**: same-origin iframe + `sandbox="allow-scripts"` (no same-origin with scripts — see `web-security`) and a `postMessage` bridge = blast-radius control.

## When NOT Pyodide

- Need > a few hundred MB of memory, GPU, or real-time → server (or `browser-ml` for inference).
- Need the latest version of a package that has no wasm wheel (common for C-heavy, bleeding-edge) → check Pyodide's repodata first; if absent and not pure-Python, it's a server feature or a rewrite.
- "Run arbitrary user Python at 10k RPS" → that's a compute service (sandboxes/firecraker), not a browser.

## Failure index (symptom → cause)

"404 on some `.js`/`repodata.json`" → indexURL relative or version mismatch (absolute + pin). "Memory climbs every run" → PyProxy not destroyed (audit `.toJs()`/kept handles; add `destroy()` in finally). "First run fast, then slower" → proxies accumulating (same) or packages loading per-run (cache). "Tab unresponsive dialog" → long `runPython` on main thread (worker + chunking). "Works in dev, fails on refresh" → FS reset (persist explicitly) or packages not preloaded.
