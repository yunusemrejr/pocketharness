---
name: api-design
description: Design API contracts, HTTP behavior, errors, pagination, idempotency, versioning, authorization and webhooks. Use for public/internal API design and compatibility reviews.
---


# API Design

## Choose the style once, deliberately

- **REST (JSON over HTTPS)** for public client-server CRUD. Default.
- **gRPC/protobuf** for internal service↔service (typed, streaming, cheap); clients in many languages is why.
- **GraphQL** only when many client types need *different* shapes from one backend (mobile+web+bots); costs: N+1 ( DataLoader/ batching), caching (HTTP cache dead per-field), and a complexity engine you must operate. A backend with one or two consumer types doesn't need it.
- **SSE** (server-sent events) for server→browser streaming (chat, progress, feeds); WebSockets only for true bidirectional high-frequency; Webhooks for service→service async push.
- Sync call-chains deeper than ~3 hops: compose in the consumer or move to queue (see `distributed-systems`).

## REST conventions (the ones that actually matter)

- Resources are **nouns, plural, hierarchical ≤ 3**: `GET /orders/42/items`, not `GET /getOrderItems`. No verbs in paths (`/users/123/archive` is OK as a state change on a sub-resource; `/archiveUser` is not).
- **Methods mean things:** GET idempotent/safe · POST create/side-effect · PUT full-replace (idempotent) · PATCH partial (define merge rules) · DELETE idempotent.
  - POST is not "any other action": `POST /orders/42/pay` (subaction) or `POST /orders/42/actions:cancel` (Google-style) — pick one pattern, apply everywhere.
- **Status codes, used honestly:** 200 ok · 201 + `Location: /orders/55` (create) · 202 + status URL (accepted async) · 204 (delete/success, no body) · 400 (bad shape, with field errors) · 401 (who are you) vs 403 (you may not) · 404 (or to hide existence) · 409 (state conflict: duplicate, version) · 422 (right shape, wrong meaning) · 429 + `Retry-After` · 5xx strictly for *your* faults.
  - 500 for "we don't want to tell you why a secret failed" is overused: 401/403/404 communicate better and leak nothing.
- **JSON shape:** pick casing (`snake_case` or camelCase — document it, stay consistent, don't mix); timestamps ISO 8601 UTC (`2026-09-06T10:15:00Z`) — never epoch-ms without docs; IDs opaque strings (no sequential int enumeration → see `web-security`); money as `{amount_cents: 1200, currency: "USD"}` or decimal-as-string — never float.
- **Filters/sort:** `?status=open&sort=-created_at&limit=50&cursor=eyJ...` — explicit, documented per endpoint (or one convention documented globally). Never ad-hoc `?field=value` combos that only the original engineer knows.
- **Pagination:** **cursor-based (keyset)** for anything large/public (stable under inserts, no deep-offset pain); offset (`?page=N`) only for small admin collections. Always return `next_cursor`/`has_more`; never let a client request unbounded (`limit` capped at, say, 200, default 50).
- **Write semantics:** PUT = full resource (missing field = cleared); PATCH = explicit merge (JSON Merge Patch is the sane default) — state which you do, in docs, per endpoint.
- **Safe-for-caching:** explicit `Cache-Control`/`ETag` on GETs (immutable after read: `ETag` + 304); mutation responses carry a fresh `ETag`/version for optimistic concurrency (see below).

## Error model (one shape, forever)

Use **RFC 9457 Problem Details** (`application/problem+json`):

```json
{
  "type": "https://api.example.com/errors/insufficient_funds",
  "title": "Insufficient funds",
  "status": 422,
  "detail": "Balance 12.00 USD, required 25.00.",
  "instance": "/orders/55",
  "errors": [{ "field": "payee", "code": "invalid_email", "message": "Not an email address." }]
}
```

- **Stable machine codes** (`type` + per-field `code`) that clients switch on — the human `message`/`detail` is free to change language/tone.
- Never leak internals (stack, SQL, paths, IDs of other tenants); log the real error server-side with a request id, echo the id in the response (`X-Request-Id`).
- Every error endpoint has a machine-readable contract in the spec, including 4xx/5xx — undocumented errors = bugs.

## Idempotency, concurrency, versioning

- **Writes that can be retried (payments!)** require an `Idempotency-Key` header: store the key → response, replay returns the stored result; keys expire (24–48 h). This is the difference between "double charged" and "retry worked."
- **Optimistic concurrency:** read carries `etag`/`version`; writes with `If-Match` → 409 on stale. Essential the moment two clients can edit the same resource.
- **Versioning:** `/v1/...` in the path (simple, cacheable, observable). Rules: **additions are free** (new optional fields, new endpoints); breaking changes get `/v2` + old version documented as deprecated with a date. Log old-version usage (headers tell you who's stuck); deprecation = 6+ months or the data says nobody.
- Never put version in the JSON body or in query params (you'll regret it).

## Authz & security at the API (deep dive in `web-security`)

- Authenticate (who) then **authorize (this object, this tenant) on every request** — including "internal" routes; a route visible is not a route allowed.
- **IDOR check:** can `/orders/<any_id>` be read by swapping the id? Tenant scoping in the query, not in the client.
- Secrets in the header (`Authorization: Bearer …`) rather than cookies for API → CSRF-immune by construction.
- Rate limiting per token + route class (auth stricter), 429 + `Retry-After`; quota per tenant for expensive ops (bulk, ML, export).

## Webhooks (when you push)

- Sign every payload (HMAC + `timestamp` header, constant-time verify; reject timestamp skew > 5 min → replay).
- Sender: retry with backoff (e.g., 5 min → 10 min → hours, over ~24 h) + dead-letter + visibility to the sender's customer. **Receiver must be idempotent** (store last processed event id per subscription) — at-least-once delivery is the contract.
- Event schema: `{id, type: "order.paid", data: {...}, created_at, signature}` — consumers switch on `type`; new event types = free, event field changes = breaking (same versioning rules).

## Spec & docs (not garnish)

- **OpenAPI is the contract** (source of truth): generated clients, contract tests (server conforms to spec in CI — `spectral`/`openapi-diff` gate for breaking-changes), and docs (Scalar/Redoc) all come from it. A doc page that drifts from the API is actively harmful.
- Realistic sample values for *every* field (realistic names, IDs, edge-case values) — "string" is not documentation.
- Code sample per language for the critical path (auth → read → write → error path).
- Changelog page mirroring the versioning rules; deprecations listed with timelines.

## Anti-patterns (instant reject in review)

`/getUsers` · 200-everything with `"success": false` in the body · unbounded lists ("just add limit later") · PATCH that silently ignores unknown fields (or 400s on them — pick, document) · enums as free-text strings for new APIs · version in the body · errors as plain-text 500s · CORS `*` with credentials · "internal" route without auth · exposing DB IDs in URLs while auth relies on those URLs·webhook without signature or without consumer idempotency · spec that's a screenshot.

## Detailed coverage

Designing public & internal APIs — REST conventions that matter, HTTP semantics done right, error model (RFC 9457), pagination, idempotency, versioning & deprecation, authz at the API, webhooks, OpenAPI discipline, and when to use GraphQL/gRPC/SSE instead. Use when designing any API, reviewing an endpoint, or fixing a broken API contract.
