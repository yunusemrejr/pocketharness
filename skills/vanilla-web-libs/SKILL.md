---
name: vanilla-web-libs
description: >-
  The no-framework (vanilla HTML/CSS/JS) library stack — the "check the platform first" rule (what's now a platform feature in 2025: <dialog>, Popover API, IntersectionObserver, ResizeObserver, Web Animations, View Transitions, scroll-driven animations, constraint-validation, Intl, loading=lazy), the maintained library picks by category (focus-trap, Flatpickr, IMask, Fuse.js, SortableJS, NoUISlider, idb-keyval/Dexie, SheetJS/jsPDF/html2canvas-pro, chart.js/d3-scales, canvas-confetti, p5/three, SplitType, Lenis, htmx, Alpine, modern-normalize, Open Props), the CDN/import-map strategy (version pin + SRI, self-host vs their CDN, graceful degrade on failure, the script-tag discipline), and the kill list (jQuery-new, the 12-micro-lib stack, the no-build tree-shake debt, the framework-reinvention threshold). Use when building without a framework (static site, CMS-embedded tool, perf-critical page), choosing which small libraries are actually maintained, or when vanilla code is about to outgrow its weight.
---

# Vanilla web libraries (the no-framework stack)

The platform + a *small named set* of libraries for what the platform doesn't cover. Two rules hold the stack:

1. **Check the platform first** (ponytail, applied to the *browser*): the 2025 platform already *is* the library for a lot — `<dialog>` (native modal, focus trap, top-layer), the **Popover API** (`popover` attribute — tooltips/dropdowns with top-layer + esc), `IntersectionObserver` (the scroll-reveal script is dead), `ResizeObserver`, **Web Animations** (`element.animate` — most micro-animation libs wrap this), **View Transitions**, CSS scroll-driven animations, **constraint-validation** (`checkValidity()`, `setCustomValidity()`), `Intl` (date/currency/number *display* + locale — the i18n-display library is the platform), `loading="lazy"`/`decoding="async"`, `details`/`summary` (the free accordion), Web Share/Clipboard, `structuredClone`. The modern-vanilla **tell**: a 2022-era script solving a 2025 platform feature (lazyload for `loading=lazy`, jquery.modal for `<dialog>`, animate.css for `@keyframes`+WAAPI).
2. **Each library needs a named job** (the micro-lib stack rule): 12 tiny libraries = the build you didn't have (12 CDNs, 12 version axes, 12 failure modes). One category of job per library; if three micro-libs would cover *one modal + one slider + one tooltip*, **write the 80 lines**. The buy-don't-write line: the *a11y interaction state machines* (focus management, combobox typeahead) are bought, never written (`component-libraries`'s rule adapted to vanilla); the styling and trivial glue are written.

## The picks (2025, maintained)

**A11y primitives** — `<dialog>` native first (feature-detect `HTMLDialogElement.prototype.showModal` for the legacy tail, never UA); **focus-trap** (the 1-file answer for non-`<dialog>` focus management — the "modal a11y broken" bug, the #1 vanilla a11y bug); **tabbable** (focusable-finder for custom focus logic); Popover API over hand-rolled `position:absolute` + z-index.

**Forms / dates / search / drag** — dates: native `<input type=date|datetime-local|month>` **first** (the mobile keyboard is the correct affordance); **Flatpickr** for range pickers / custom calendars / server-string-heavy locales (the maintained classic; the `air-datepicker` line is the unmaintained tell). In-field masks: **IMask** (the `inputmask`-of-2019 is legacy). Client fuzzy search: **Fuse.js** (the server-search path is the backend's job — `api-design`). Drag-reorder: **SortableJS**; column sort: 40 hand-rolled lines, **do not skip `aria-sort`**. Sliders: native `range` for single-value, **NoUISlider** for range/multi-handle (headless).

**Data / storage / export** — IndexedDB: **idb-keyval** (default, 10 lines), **Dexie** (structured stores), localforage only for a legacy fallback chain. localStorage for flat small data. The client-DB is a *local/cache*, not the source — the sync story is the app's, not the library's (`databases` line). Export: **SheetJS** for xlsx — **trap: the npm `xlsx` is stale** (SheetJS ships via its own CDN/registry; pin the *current* release, not the default-registry one); **jsPDF** for custom-layout PDF; **html2canvas-pro** for DOM→image (the *original* html2canvas breaks on `color-mix()`/`oklch` in computed styles — the pro fork is the 2025 requirement if your CSS uses modern color functions); and the underrated zero-dependency answer: `window.print()` + `@media print` covers *most* "export as PDF".

**Canvas / viz / 3D** — **chart.js** (the vanilla default, batteries included); **d3 *pieces*** for custom grammar (`d3-scale`, `d3-shape`, `d3-array` — full-d3 import is the 280KB, the modular ESM is ~30KB; which *chart* is `data-viz`'s call); **canvas-confetti** (the confetti, count-capped, 2s lifetime — `web-effects` line); **three.js** (the `threejs` skill; version pin *mandatory* — the 0.x majors break); **p5.js** for creative sketches (production canvas is plain `canvas` 2D + rAF); **Matter.js**/p2.js only for the named physics job.

**Effects / scroll glue** — **SplitType** (per-char/word spans for stagger reveals) + WAAPI for the animation (the `animate.css`/`typed.js` line is the 2019 tell); **Vanilla-Tilt** or a 30-line tilt (`web-effects`); **Lenis** for smooth scroll (the `animation-libraries` anchor/keyboard caveat applies unchanged).

**Server-driven & framework-lite (know the boundary)** — **htmx** (+ Hyperscript): the server returns HTML, partial swaps — the *correct* answer for content-driven apps (feeds, comments, forms-to-server) that want interactivity without the SPA state machine; the *wrong* shape for state-driven apps (collaborative, heavy local state — that's the SPA, `frontend-js` line). **Alpine 3**: DOM reactivity without the build — right for component-lite interactivity (disclosures, small form state); the threshold: when it grows routing + a global store + template diffing, it's a reinvented framework and the honest move is a real small framework (Solid/Preact/Vue). The **no-build toolchain**: import maps + ESM CDNs (esm.sh / jsDelivr `+esm`) for modules, a static server for dev, **modern-normalize** (the 1KB reset — `normalize.css` is legacy) + **Open Props** (the token layer: `--size-*`/`--font-*`/`--radius-*`/`--shadow-*` as CSS custom properties) as the style foundation. The catch: **tree-shaking is your job** (the whole-d3 import, the whole-three import).

**Icons** — the SVG sets (**Lucide** / **Phosphor** / **Tabler** / Material Symbols): one family per product, `currentColor` (the icon-font is the 2019 tell, recolor dies with it — `svg-assessment`).

## The CDN strategy

- **Version pin + SRI on every remote**: `https://cdn.jsdelivr.net/npm/chart.js@4.4.1/…` + `integrity="sha384-…"` + `crossorigin="anonymous"` (the `@latest` URL is the "worked last week" incident; the missing SRI is the script-injection vector — `web-security`).
- **Self-host vs their CDN**: default to their CDN (uptime + edge cache). Self-host for (a) intranet/air-gapped, (b) high-privacy products (every third-party CDN request is a third-party data channel — IP/UA exfil; GDPR-sensitive surfaces get the self-host), (c) low-trust targets. SRI on self-hosted builds too, re-hashed per build.
- **Graceful degrade on CDN failure**: the page renders and core jobs work with the library 404. Critical-path libs get feature-gates (`if (window.Chart) … else static`); decorative libs get `onerror` → the still fallback (`web-effects` still-frame rule). The hard-crash-on-CDN-404 is a release blocker, not a known limitation.
- **Script discipline**: `type="module"` (default-deferred + scoped) / `defer` classic / `async` only for independent (analytics-class); `preconnect` to the CDN domain; `modulepreload` for the critical module; one module graph, not 15 classic tags.
- **Polyfill policy (2025)**: rarely needed. Feature-detect, never UA-sniff (`wasm-browsers` line). The polyfill tower is the 2016 answer; the 2025 answer is platform + feature-gate + graceful degrade.

## The kill list

- **jQuery in a new project** (the legacy-maintenance exception is the only correct 2025 jQuery — strangler line, `component-libraries`).
- **The 12-micro-lib stack** — the named-job rule enforcement (the 80-line script beats the 3-CDN surface).
- **The build-less tree-shake debt** — importing whole libraries into a no-build page *is* the build cost, paid in bytes; modular imports or a zero-build static stack.
- **The framework-in-disguise** — vanilla app doing its own routing + store + diffing (or grown Alpine) → the honest switch to a small framework, not the hybrid.
- **Unpinned, unverified CDNs** (the security + stability pair, above).
- **The 2022-lib-for-the-2025-platform** (lazyload / dialog-plugin / animate.css / `Intl`-replacing date *display* formats — `Intl` for display, `date-fns`/`dayjs` only for date *math`).
- **The unmaintained classic** — the 18-months-without-a-substantive-commit check (dead: air-datepicker, the original html2canvas for modern color CSS, the stale `xlsx` npm path).

## Failure index

- *Modal a11y broken (focus escape, no esc, SR-invisible)* → hand-rolled modal → `<dialog>` or `focus-trap`; the state machine is bought.
- *Date UI fights mobile / i18n* → custom date field → native first, Flatpickr for range/locales, `Intl` for display.
- *CDN 404 on the intranet / privacy review flags the third-party script* → self-host + SRI re-hash.
- *15 `<script>` tags, 12 upgrade axes* → the named-job consolidation + the module graph (import map).
- *Export-PDF breaks on the modern color CSS* → original html2canvas → html2canvas-pro, or `@media print` (the zero-dep most cases).
- *`npm i xlsx` is two releases behind* → SheetJS's own registry/CDN pin.
- *280KB d3 for one scale* → modular ESM imports (the no-build tree-shake is manual).
- *Vanilla app now has a router + a store + a diff* → the framework threshold → the small framework (the hybrid is the worst of both).
- *Lenis smooth-scroll breaks anchor links / keyboard* → anchor/focus through its API, or native scroll (the a11y floor).
- *"Can I use `<dialog>`?"* → yes in 2025; feature-detect the legacy tail; the fallback is `focus-trap`, not a polyfill tower.
