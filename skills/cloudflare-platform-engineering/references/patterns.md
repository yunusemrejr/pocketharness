# Cloudflare Platform Engineering: patterns and examples

## Edge and origin contract
Workers runtime support is not identical to Node.js. Check compatibility date/flags, bindings and current CPU/memory/subrequest limits. Keep secrets in appropriate secret bindings and avoid logging credentials. Local emulators do not prove production network, rate-limit or storage behavior. Use the platform's supported lifetime mechanism for background work; fire-and-forget promises may not survive request completion.

Choose storage by consistency and access pattern: do not assume KV, D1, R2, Durable Objects and Cache API share transaction or replication semantics. Identify a single owner for coordinated mutable state. Validate migration order and rollback against live bindings before publishing.

## Turnstile
A widget success is not server authorization. Send its token from the client to your backend, then verify using the official Siteverify endpoint and server-held secret. Check `success` plus expected hostname/action where appropriate, handle expired/duplicate tokens, and return a recoverable client flow. Tokens are single use; do not blindly replay a verification after an uncertain network result. Use the documented idempotency mechanism where applicable. Official test keys exercise integration paths, not production abuse resistance. Turnstile does not replace login, authorization, CSRF defenses or business rate limits.

## Cache and API safety
Define exactly which responses are public. Never cache personalized output under a shared key merely because a URL matches. Consider cookies, Authorization, query normalization, locale, Vary and purge ownership. A cache bypass incident should have a bounded purge strategy; avoid global purges as a default development action. Test authenticated versus anonymous and tenant A versus tenant B responses.

Use token-scoped API calls with deadlines and bounded retries for safe operations. Reconcile uncertain writes before retrying; respect rate-limit feedback and pagination. Record account/zone/resource identity so an origin URL is not mistaken for an edge resource. Validate error, cache-hit/miss and expired-token cases using fixtures, then state explicitly which production behaviors remain untested.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://developers.cloudflare.com/workers/
- https://developers.cloudflare.com/turnstile/get-started/server-side-validation/
- https://developers.cloudflare.com/turnstile/troubleshooting/testing/
- https://developers.cloudflare.com/api/
