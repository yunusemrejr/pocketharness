---
name: modern-frontend-frameworks
description: "Build with React, Vue, Svelte, Next.js, Nuxt, and SvelteKit beyond vanilla JS: server components, SSR/SSG/ISR, hydration, islands, state and data patterns, plus API, auth, and session interop with PHP, Node.js, Flask, and Django backends."
---

# Modern Frontend Frameworks

Use when frontend work involves React, Vue, Svelte, Next.js, Nuxt, SvelteKit, or similar — or when a framework app must coexist with vanilla JS, server-rendered pages, or PHP, Node.js, Flask, or Django backends. For vanilla browser JS, DOM behavior, and Alpine/jQuery integration use browser-javascript-engineering and frontend-js; for component-library adoption tradeoffs use component-libraries; for stack selection use web-ui-stack-selection.

## Working method

- Name the rendering model first: client-rendered SPA, SSR, SSG, ISR, or islands. The model decides where data loads, where secrets may live, and what hydration must reproduce — most framework bugs are model confusion.
- Keep server and client code separated by construction (server components versus client components, loaders versus effects), not by convention. Anything imported by the client ships to the browser.
- Treat the backend as a versioned contract: typed API shapes, explicit auth and session behavior, documented error envelopes. Framework and backend evolve on different cadences; the contract is what keeps them compatible.
- Verify in the real delivery shape: production build, production data volume, throttled network, and the actual backend — not only the dev server with fixtures. Dev-server behavior (error overlays, unminified bundles, lenient timing) hides production defects.

Read [framework patterns](references/framework-patterns.md) for rendering models, data, state, and hydration. Read [backend interop](references/backend-interop.md) when the frontend talks to PHP, Node.js, Flask, Django, or mixed vanilla pages; do not load it for pure client-side component work. Secrets live on the server or in the OS environment — never in client bundles, frontend env files shipped to browsers, or URLs. User instructions take precedence; this skill adds no authority to change backends or deploy.

## Evidence and completion

Report the rendering model, the contract surface touched, and what was verified (build, routes, data states, error states, auth flows) with production-like evidence. Name framework-version assumptions and unverified combinations (browser, backend version, network condition). Do not present dev-server behavior as delivery verification.
