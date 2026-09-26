# Change-driven discovery

Reuse the CMS or deployment pipeline before adding a new service. Build a manifest of canonical public URLs and meaningful content hashes. Ignore build timestamps and irrelevant chrome changes. After deployment succeeds, enqueue added, materially updated and deleted URLs; do not submit previews, private paths, authentication tokens or arbitrary user-supplied hosts.

Generate XML sitemaps from canonical indexable URLs. Use truthful significant-modification timestamps; omit unknown `lastmod`. Publish the sitemap location in `robots.txt` and submit through authorized webmaster tooling where needed. [Google retired its sitemap ping endpoint](https://developers.google.com/search/blog/2023/06/sitemaps-lastmod-ping); repeated unauthenticated pings do no useful work.

[IndexNow](https://www.indexnow.org/documentation) supports added, updated and deleted URLs on a verified host. Its key file proves host control; validate the deployment hostname and key location before enabling delivery. Prefer an existing CMS integration. Notify one participating endpoint because participating engines share submissions, as described in the [IndexNow FAQ](https://www.indexnow.org/faq). Never treat acceptance as an indexing guarantee.

Use an outbox keyed by `(canonical URL, content version, operation)`, coalesce rapid edits and enforce provider batch limits. Retain failed events across restarts, retry transient failures with backoff and jitter, respect `Retry-After`, and surface persistent ownership/validation failures without blocking publication. A retry must not create a notification storm. Apply a host allowlist and HTTPS; do not fetch arbitrary URLs from content to validate submissions.

Deletion removes a URL from the sitemap while still permitting a supported deletion notification. Confirm the final public response or intentional redirect before submitting; do not send a deletion for a temporarily unavailable deploy.

[Google's Indexing API](https://developers.google.com/search/apis/indexing-api/v3/using-api) is restricted to pages with `JobPosting` or `BroadcastEvent` embedded in `VideoObject`, with its required access. Do not apply it to ordinary blogs, products or FAQ pages.

Test: unchanged rebuild produces no notifications; one article edit produces one event; retry stays bounded; failed deploy emits nothing; private routes never enter the manifest; deleted page leaves the sitemap. Record accepted requests separately from observed indexing.

Protocol sources inspected 2026-09-10; check current limits before implementation.
