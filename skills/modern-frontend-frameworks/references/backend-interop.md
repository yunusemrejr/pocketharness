# Backend interop

## API contracts

Define the contract as data both sides validate: request/response shapes, required versus optional fields, date and money formats (ISO strings and minor units beat locale formats), pagination envelopes, and a documented error shape (machine-readable code, human message, per-field details where relevant). Generate or share types from one source (OpenAPI, schema-first codegen) rather than hand-mirroring shapes in two repositories.

Version the contract before breaking it: additive changes flow freely, breaking changes need a versioned path or a coordinated deploy window. Deprecate explicitly with sunset dates and consumer inventory — an API with unknown consumers cannot evolve safely. Test the frontend against contract fixtures and the real backend; fixtures alone prove only fixture compatibility.

## Auth and sessions across the boundary

Pick one session story and implement it completely:

- Cookie sessions (classic PHP, Django, Flask-Login): the backend owns identity; the frontend sends credentials (`credentials: "include"` semantics) and handles 401 by redirecting to login. CSRF protection is mandatory for cookie-authenticated mutations — same-site attributes plus per-request tokens where the framework requires them.
- Token auth (API-first backends): short-lived access tokens with secure refresh, stored where XSS cannot reach them (httpOnly cookies beat localStorage). Never store tokens in localStorage alongside XSS-reachable scripts and call it secure.
- SSR backends (Next.js/Nuxt/SvelteKit server code calling APIs): server-to-server calls carry service credentials from the OS environment — never from client-visible code. The SSR server is a backend with backend secret rules.

Never place secrets in client bundles: no API keys, no service tokens, no connection strings in frontend code, frontend `.env` files that ship to browsers, or URLs. Public endpoints that need keys get a backend proxy (BFF route) holding the key server-side.

## CORS, cookies, and deployment topology

Cross-origin deployments (app on one host, API on another) need deliberate CORS: exact allowed origins (never reflected origin), credentials mode matched on both sides, and preflight coverage for the methods and headers used. Same-site cookie attributes interact with the topology — test login, refresh, and logout across the real domains, not just localhost where browsers are lenient.

Prefer same-origin delivery (reverse proxy routing `/api` to the backend) when possible: it removes CORS, simplifies cookies, and matches how users actually reach the app. Whatever the topology, verify it in staging with production domains and TLS — localhost success is weak evidence.

## Coexisting with vanilla and server-rendered pages

Framework apps rarely own every page. Integration patterns that survive:

- Islands in server pages: mount one component per interactive region (PHP template, Django template, or static HTML) with props from data attributes or embedded JSON. Each island is independently deployable and testable.
- App shell with legacy routes: the framework router owns new routes; legacy server routes keep working untouched. Share only session/auth and global styles, versioned carefully.
- Micro-frontends: separate framework apps per domain area, composed at deploy or runtime. Powerful and expensive — adopt only when team independence genuinely requires it, with shared dependency discipline (one React/Vue/Svelte instance per page, not three).
- Progressive enhancement: server-rendered content first, framework behavior layered on. Every enhancement must fail safe to the working baseline.

Shared-state bridges (custom events, a tiny vanilla store, URL params) beat deep framework coupling across these boundaries. Keep the seam narrow and documented.

## File uploads, errors, and edge cases

Uploads need size limits, type validation on the server (MIME sniffing, not extension trust), progress reporting, and resumability for large files. Error envelopes must survive proxies and error pages: validate the shape on malformed-HTML responses too, since a gateway error page is not JSON. Timeouts, retries (idempotent mutations only), and offline behavior belong in the contract from the start, not as incident follow-ups.

## Primary references

- https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
- https://owasp.org/www-project-web-security-testing-guide/
- https://nextjs.org/docs/app/building-your-application/authentication
- https://docs.djangoproject.com/en/stable/topics/auth/
- https://flask-login.readthedocs.io/
