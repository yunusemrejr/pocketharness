---
name: web-performance
description: Web performance engineering — Core Web Vitals budgets, rendering pipeline, bundle/image/font optimization, code splitting, caching, and measurement with DevTools/WebPageTest. Use when a page is slow, auditing performance, or setting budgets before building.
---

# Web Performance

## Budgets (defaults unless the product says otherwise)

| Metric | Good | Warn |
|---|---|---|
| LCP | < 2.0 s | < 2.5 s |
| INP | < 200 ms | < 300 ms |
| CLS | < 0.1 | < 0.25 |
| TBT | < 150 ms | < 300 ms |
| First load transfer | < 150 KB JS (gz) + < 200 KB critical assets | < 300 KB |
| Route-level JS delta | < 50 KB | |

Encode budgets in CI (bundlesize/lighthouse-ci/size-limit) — unmeasured budgets rot.

## The rendering pipeline (order of what to optimize)

1. **Network**: round-trips before bytes. Minimize DNS/TCP/TLS (fewer origins, HTTP/2+), preload critical, defer the rest.
2. **Parse/compile**: JS bytes cost parse + compile + execution. Delete, don't minify, into health.
3. **Layout/paint**: forced reflows, oversized images, no CLS reservation.
4. **Main thread**: long tasks (> 50 ms) → INP pain.

Diagnose in the same order; fixing #3 while #1 is broken is wasted work.

## Bundle

- `npx why-bundle` / `source-map-explorer` / bundler analyzer: top 10 modules by size, question each.
- Code split by **route and by interaction** (load charts only when the tab is opened). Dynamic `import()` for pdf/excel/xlsx, editor, 3d, ml — all of them.
- Deps: one utility dep = permanent tax. Re-implement in 10 lines if it's map/filter territory.
- Frameworks: verify the framework runtime itself isn't double-bundled (duplicate React copies from two npm trees = classic).
- Tree-shaking defeats: barrel files, side-effect imports, CommonJS in an ESM chain.
- Target: keep initial JS < ~100 KB gz on landing; every extra KB of parse on a 4G phone is measurable p75.

## Images

- AVIF (fallback WebP) via `<picture>` or `srcset`; raster for photos, **SVG for anything geometric** (icons, logos, illustrations with ≤ a few paths).
- `width`/`height` (or `aspect-ratio`) on every image — zero CLS from images.
- `fetchpriority="high"` on the LCP image, `loading="lazy"` below the fold, `decoding="async"`.
- Responsive: `srcset` at 1×/2×, don't ship a 4K hero to phones.
- Optimize before caching: sharp/squoosh presets (AVIF q50–60 for photos is invisible below ~80 KB).

## Fonts

See `fonts` skill. Performance lens: one woff2, subset, `swap`, preload primary, no 4-weights-per-family.

## JavaScript & main thread

- INP comes from long tasks: profile with Performance panel (10× slows, looks for > 50 ms tasks, look at their callees).
- Defer non-critical: `requestIdleCallback`/`setTimeout` chunk, `scheduler.yield()` in React 19, Web Worker for heavy compute (JSON parsing of large payloads alone can cause a 200 ms jank).
- Third-party scripts are the #1 uncontrolled long task: self-host analytics, audit every iframe/tag manager script, load after interaction where possible.
- `content-visibility: auto` + `contain-intrinsic-size` for long lists/sections — free INP + paint wins.
- Hydration: islands/`next/dynamic`/React `useDeferredValue` — don't hydrate navs and footers.

## Caching

- Immutable hashed assets: `Cache-Control: public, max-age=31536000, immutable` (bundlers emit hashed names).
- HTML: `no-cache` + ETag (or short `s-maxage=60` at edge) so deploys propagate.
- API: `stale-while-revalidate` for feed-like data; ETag/304 for document-y endpoints.
- Service worker only when offline is a product requirement, and SW cache-bust strategy must be explicit (versioned precache, versioned).

## Measurement

- **Real users**: RUM (web-vitals lib + your analytics) — lab Lighthouse is a floor, not the truth; report p75.
- **Lab**: Lighthouse CI in PR (regression gate), WebPageTest for 4G/Moto-class throttled runs on real pages, and **Performance panel for "why"**.
- Bisect regressions with `git bisect run lighthouse-ci` or bundle-size gates per PR.

## Anti-patterns

- "Lighthouse 95 but users complain" → usually RUM shows p90 LCP 6 s from one slow region/dep; look at p75/p90, not lab median.
- Preloading everything = network thrashing; preload only the ONE next-likely asset.
- Adding a CDN to an already-fast static site: extra TLS handshake can net-negative.
- Optimizing a fast page because the tool says 87/100 — verify the metric the user feels (INP/LCP) before chasing the score.