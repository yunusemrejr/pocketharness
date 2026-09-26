# Framework patterns

Framework APIs move fast; verify every pattern below against the installed version's own documentation before applying it. The principles transfer, the import paths often do not.

## Rendering models

- Client-rendered SPA: the server ships a shell and the client fetches everything. Simple to deploy, weakest first paint and SEO, full loading-state burden on the client. Choose it for authenticated apps behind login, not for public content.
- SSR: the server renders each request. Fast first paint with real data, at the cost of server load and serialization discipline (everything rendered must cross the server/client boundary cleanly).
- SSG/ISR: pages pre-rendered at build or revalidated on interval. Best for stable public content; dynamic islands inside static pages cover the interactive parts. Verify revalidation behavior under load, not just in docs.
- Islands and partial hydration: static HTML with isolated interactive components. Minimal JavaScript by default; each island owns its state and loading. Ideal for content sites with pockets of interactivity — and the natural meeting point with vanilla JS (see backend interop).
- Server components (React/Next.js App Router): components that render only on the server, fetching data without client waterfalls or client-side secrets. Client components handle interaction. The boundary is the architecture: misplaced `"use client"` directives silently ship server code (and sometimes secrets) to browsers. Audit the boundary, don't assume it.

## Data loading

Fetch where the model says: server loaders/actions for SSR frameworks, route-level fetching over component-level waterfalls, cached and deduplicated requests over repeated effects. A component tree that fires N sequential requests on mount is a loading waterfall — lift fetching to the route, parallelize, or stream with Suspense-style boundaries.

Handle the full state matrix per data surface: loading, empty, error, stale-while-revalidating, and unauthorized. Skeletons must match final layout to avoid shift; error states must offer recovery, not dead ends. Test slow and failing networks explicitly — the happy path is the minority of real sessions.

Mutations need lifecycle handling: optimistic updates with rollback, pending states that prevent double-submit, and cache invalidation scoped to what changed. A mutation that succeeds on the server but leaves the client cache stale is a user-visible defect.

## State ownership

Own each piece of state once: server state in the data layer (query cache), ephemeral UI state in components, shared client state in the minimal store that fits. Duplicating server data into client stores creates sync bugs; derive instead of duplicating. URL state (search params, route segments) owns shareable view state — if refreshing loses it, it lived in the wrong place.

Effects are for synchronization with external systems, not for data flow orchestration. An effect that fetches on mount, chains dependent fetches, or mirrors props into state usually signals a modeling problem the framework already solves declaratively.

## Hydration discipline

Hydration replays server HTML on the client; any mismatch (dates, random values, locale-dependent formatting, browser-only APIs during render) breaks or degrades it. Render deterministically on both sides: same data, same locale, same timezone, no `Math.random()` or `Date.now()` in render paths. Defer browser-only rendering explicitly (dynamic import with SSR disabled, mounted guards) rather than suppressing mismatch warnings.

Measure hydration cost: large component trees hydrate slowly on weak devices even when the HTML paints fast. Code-split by route, defer below-the-fold islands, and verify interactive timing on throttled hardware profiles — Lighthouse on a developer workstation is not the user experience.

## Build and bundle discipline

Analyze the production bundle, not the source tree: route-level chunks, duplicated dependencies (two versions of one library is a dependency-resolution bug), and accidental server-code inclusion in client chunks. Set bundle budgets per route and fail CI on regression. Tree-shaking, side-effect flags, and import discipline (`lodash-es`-style precision over barrel imports) keep payloads honest.

## Primary references

- https://react.dev/
- https://nextjs.org/docs
- https://vuejs.org/guide/
- https://nuxt.com/docs
- https://svelte.dev/docs
