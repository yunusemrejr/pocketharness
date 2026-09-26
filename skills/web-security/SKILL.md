---
name: web-security
description: 'Implement proportionate web security: authentication, authorization, input/output handling, browser defenses, uploads and API abuse protection. Use for web vulnerabilities and security reviews.'
---


# Web Security (practical, proportional, non-locking)

## The thesis

Two failure modes, both fatal: **gaps** (top-10 vulns that ship) and **over-engineering** (a CSP that breaks the checkout, an admin lockout, five scanners blocking every PR, home-rolled crypto). Security is **risk-proportionate layering**: match the controls to what can be stolen/damaged, and make every control *operable* (reversible, documented, recoverable). A control the team can't operate is not a control — it's a ticking incident.

## Step 0: tier the risk (5 minutes, decides everything)

| Tier | System | Regime |
| --- | --- | --- |
| **T1 public SaaS** | credentials, money, PII, multi-tenant | full: everything below, end-to-end, tested |
| **T2 public tool** | anonymous use + API keys/rate limits, no PII at scale | OWASP core + rate limits + dependency hygiene; auth where accounts exist |
| **T3 internal / marketing** | intranet, static site, docs | TLS + headers + dependency/secret scanning + 3rd-party script audit; no home-rolled anything |

Over-provisioning T3 (mTLS, per-request signing, per-tenant KMS) and under-provisioning a T1 payments path ("auth later") are the same error in opposite directions. **Write the tier in the ADR** — it's what you'll be argued about in 6 months.

## The crypto baseline (all tiers, all of it, none of it optional)

- **TLS everywhere**: TLS 1.2 minimum (1.3 default), HSTS (`max-age=31536000; includeSubDomains`, `preload` at T1), no mixed content.
- **Never roll crypto**: no custom "MAC", no "our hash with our salt". HMAC-SHA256 / AES-GCM / Ed25519 via a vetted lib (Web Crypto API / libsodium / stdlib). The "we just need a checksum" that becomes an integrity check in 2 years is the classic.
- **Random**: `crypto.getRandomValues` / `crypto.randomUUID()` / `secrets.token_urlsafe` — **never `Math.random`** for any token (IDs get sequential, sessions get guessed).
- **Passwords**: **Argon2id** (or bcrypt cost ≥ 12) — never md5/sha1/"sha256 + salt" (that's md5 with steps). Length ≥ 12 enforced where you create password auth, plus **breach-list check** (k-anonymity HIBP) — length + breach-check beats 8-char-with-symbol theater.
- **Sessions**: server-side session, token = 128+ bits random, **stored hashed** (DB holds the hash: a leaked DB ≠ stolen sessions), `HttpOnly; Secure; SameSite=Lax` cookie path (below), absolute + idle expiry, **revoke on login-from-new-device / password change / logout**.
- **Secrets**: env/vault at runtime, **never in the repo** (gitleaks pre-commit + CI), **never in the client bundle** (grep your production bundle for `sk_`/`api[_-]?key`/`secret` on every release — a 2-minute audit that has saved products), rotation plan per secret (who rotates, when, what breaks).

## OWASP as implementation notes (not a checklist)

- **XSS**: modern framework templating auto-encodes — **trust it, and don't fight it**: no `innerHTML`/`dangerouslySetInnerHTML`/`v-html` with data (if rich text is product, sanitize: DOMPurify allowlist). **CSP** (below) limits but does not fix XSS — layering, never substitute. `X-Content-Type-Options: nosniff` (2 lines, legacy but real), `Referrer-Policy: strict-origin-when-cross-origin`, `Permissions-Policy: camera=(), microphone=(), geolocation=()` (deny-by-default for capabilities you don't use).
- **CSRF**: `SameSite=Lax` cookies = done for 90% (an attacker-origin form post carries no cookies); for the rest (or `None` cookies you actually need), custom-header check or double-submit. **APIs that authenticate via `Authorization: Bearer` header are CSRF-immune by construction** — prefer that for anything state-changing (a real simplification, not a "modern" flex).
- **Injection**: **parameterized queries, always** (`$1` / `%s` / ORM bindings); ORM `raw()` = an escape hatch that needs review; **no shell exec with input** — `shell: true` + interpolation = RCE, "sanitized" is not a boundary (allowlist commands, pass args as an array); SQL: the ORM/parameterization is the control, "we escape" is a rumor.
- **SSRF**: user-supplied URL that your server fetches = verify **before** fetching: allowlist domains first (the sane model); if you must fetch arbitrary hosts, resolve → reject private/link-local (`10/8, 172.16/12, 192.168/16, 169.254/16` ← **cloud metadata endpoint**, the classic RCE-to-secrets, `::1`, `0.0.0.0`, `fc00::/7`), **no redirects to non-allowlisted hosts** (redirect = re-validate), and a per-domain rate cap. Same rule in-browser: user triggers fetch → proxy through a server that enforces this (see `wasm-python` sandbox note).
- **IDOR (the #1 real-world leak)**: can you read `/orders/<any_id>`? Every object access = **tenant + ownership check in the query** (not in the client, not in the route); **opaque IDs** (uuidv7/`nanoid`) in URLs — sequential ints + a route that "trusts the viewer" = an enumeration script; per-tenant scoping is a data-layer invariant (the query can't even see other tenants' rows), not a middleware.
- **Authz**: server-side, per request, per object — "admin role can hit this route" is not object authz (admin A reading tenant B's invoices is still a leak). **Fail closed**: the check errors → deny, never allow. Admin actions = **audited** (who/what/when/where), and see Lockout-prevention (below) — admin that can't be reached is a P0.
- **Uploads**: type by **content** (magic bytes), not extension; **re-encode images on ingest** (kills polyglots: a `.jpg` that's actually a polyglot SVG/JS, ImageMagick policy, PDF via a renderer sandbox — never `office --convert` on user files without container isolation); size caps **before** processing; stored **outside the webroot** (or a bucket with `content-disposition: attachment`, no inline, no execution policy); zip: expand to a cap, reject `..` paths (zip-slip), cap the decompression **ratio** (zip bomb: 10:1 max, abort on exceed).
- **Mass-assignment / deserialization**: bind explicit fields (never `{...req.body} → user`); JSON is fine (a typed decoder is better); **`pickle`/`yaml.load` (unsafe loader)/custom `JSON` decoders on user input = RCE** — `yaml.safe_load` only; file formats (Office/PDF) always in a sandboxed process with a timeout.
- **Rate limiting & abuse**: per route class — auth strict (login: 5/min/account + 20/min/IP, backoff + **no user enumeration**: identical timing/message for unknown user), general 100/min/token; **429 + `Retry-After`** (not silent drops); per-tenant quotas on expensive ops (bulk, export, ML, OCR); **untrusted work is queued + resourced** (time-boxed, CPU/mem-capped) — a user-triggered job that can take 3 h is a DoS you designed.

## CSP that doesn't lock you out (the explicit ask)

CSP's value: it narrows the blast radius of any XSS (no exfil to `evil.com`, no eval, no remote script) — and its failure mode is **breaking your own app** (the checkout "just stopped working" because the payment iframe isn't in `frame-src`). The procedure is the product:

1. **Start in `Content-Security-Policy-Report-Only`** + a `Report-To`/`report-to` endpoint (you get a free log of every violation for a week).
2. **Write the policy from the violations**, not from a template: `default-src 'self'; script-src 'self' [nonce]; style-src 'self' 'unsafe-hashes'?; img-src 'self' data: https://cdn…; font-src 'self' data:; connect-src 'self' https://api…; frame-src https://checkout…; frame-ancestors 'none' (or 'self'); object-src 'none'; base-uri 'self'; form-action 'self' https://checkout…; upgrade-insecure-requests`.
3. **No `'unsafe-inline'` for scripts** (it nullifies most of the XSS value). Bootstrap configs / inline bits → **per-request nonce** (frameworks: Next head, SvelteKit, Express `helmet.nonce`) or move to a file. Dynamic bundles → nonce + `strict-dynamic`.
4. **Third-party scripts**: proxy first-party (analytics) or delete — "CSP broke GA" is a **decision in a meeting** (proxy the tag or lose it), never a permission to add `'unsafe-inline'`.
5. **Enforce**, watch reports for 2 weeks, tighten what the reports say you can tighten; the report-only phase is what keeps you from the 3am "prod is broken" of a guessed policy.

- Mix note: `script-src 'self'` + a nonce → the nonce wins (`'self'` ignored for scripts under CSP3 with nonces) — use `strict-dynamic` when you have nonces. `frame-ancestors`/clickjacking: `'none'` unless the product is embedded (then `'self'`/explicit origin — and that embedding is the attack surface, document it).
- **`sandbox` for iframes of untrusted content**: `sandbox=""` (deny everything) + explicit `allow-*` per need; **`allow-scripts` and `allow-same-origin` together = the iframe can remove the sandbox** — for hostile content, never both (use a separate origin for the script).

## AuthN patterns (the decisions that matter once)

- **Session (default for web apps)**: server-side, revocable, the browser doesn't hold logic. Statelessness is not a virtue — it's a constraint (no "log out everywhere" without an extra store).
- **JWT**: legitimate for stateless APIs / mobile-offline / cross-service claims — keep access ≤ 15 min + **refresh rotation** (reuse of an old refresh = revoke the family), and never put PII in a JWT (it's base64, signed, **not encrypted**).
- **2FA**: TOTP at minimum for admin/sensitive; **passkeys/WebAuthn** where the users are on modern devices (less friction than TOTP codes — friction on the safe path is where "disable MFA" tickets come from). 2FA **change/removal** and **password reset** = the high-friction points (the real bypass paths).
- **Password reset**: the classic bypass factory — reset tokens single-use, short TTL (15–30 min), sent to the *verified* channel, **invalidate all sessions on reset**, rate-limit + no enumeration (same response for unknown email), and the "email" is never the only factor if the user has 2FA (the "change email" flow needs the current session to survive + a confirmation in the *old* inbox where possible).
- **SOC2 / auditor note**: audit log (auth events, admin actions, data access at T1) immutable-ish (append-only, no delete), retention per policy — the audit *log* is usually more load-bearing than anyone's home-rolled control.

## Dependency & supply chain (T1 full, T2/T3 core)

- Lockfiles **committed** + `npm ci`/`pip install --require-hashes`/`cargo` lock — reproducible installs, ever.
- **`npm audit` / `pip-audit` / `cargo audit`** in CI (advisory = triaged, not blindly green-lit: a "critical" with no reachable path is noise; a "moderate" on your exact path is real). Dependabot/Renovate for the actual patching (you won't, manually, forever).
- **Native build scripts are the supply-chain RCE** (`postinstall` runs arbitrary code): audit them, `--ignore-scripts` where you can *and* the deps you need don't require them (native addons do — pin + audit those), and pin the **entire** toolchain (node/python versions in `.tool-versions`/`pyproject`) so "reproducible" means it.
- SBOM (CycloneDX/SPDX generated in CI): **not theater at T1** (a vulnerability day with an SBOM is a 20-minute triage; without, a 3-day panic) — but an SBOM nobody reads at T3 is theater; generate it if anyone downstream consumes your artifact.
- Provenance/signing (Sigstore/cosign): **adopt under policy** (publishing to a registry others install from); signing an internal artifact no one installs is theater.

## Lockout-prevention (where over-engineering hurts you)

- **Every new control ships with its failure path**: MFA enforcement has an exempt, *narrower-scoped* service identity (not a global off-switch); rate limits have a documented raise path + an owner; CSP report-only before enforce; 2FA has a recovery code *that was tested* (the recovery path gets exercised in staging quarterly — **the break-glass drill is a feature, not an afterthought**).
- **Admin reachability is a security control, not a bug**: separate admin path + IP allowlist (where truly needed) + **documented break-glass** (a known person, a known phone call, a known runbook) — the "admin is locked out" incident is a P0, and the only fix that matters is the drill.
- **Feature-flag risky controls**: rate-limit values, MFA enforcement, CSP strictness — behind flags you can dial in prod without a deploy (a 99th-percentile lockout at 3am becomes a 1-minute flag flip).
- **Friction doctrine**: every second of friction on the *legitimate* path (login, checkout, "recover account") is a support queue + an attack-surface alternative (users will disable MFA you made annoying — friction you designed is the backdoor). Attack paths get the friction (2FA, backoff, checks); happy paths stay smooth (passkeys).
- **If a control is in prod for 6 months and nobody has touched its knobs**, it worked — keep it. The over-engineering tell is **controls with knobs nobody has ever turned** *and* a runbook nobody has ever run: those are either the right level (keep, simplify docs) or the wrong level (delete, replace with the measured risk).

## Verify without paranoia (the pragmatic test set)

- **CI, every PR**: secret scan (gitleaks), dependency audit, `semgrep`/CodeQL **default rulesets** (custom SAST rules = theater until a specific vuln class is proven; the defaults catch the real class: SQLi, SSRF, injection), CSP report-only collection.
- **Staging, per deploy + weekly**: ZAP/nuclei **baseline scan** (finds the misconfigurations; you're not finding 0-days, you're finding the missing `frame-ancestors`), **your 10-item hand checklist on the changed surface**: (1) SQLi on one string input · (2) XSS on one reflected input (and the rich-text one) · (3) IDOR pair (user A's object id → user B's session) · (4) authz bypass (guest hitting admin route, self-host vs other-tenant) · (5) SSRF on the URL field (metadata endpoint, `127.0.0.1`, redirect to internal) · (6) upload polyglot (`.jpg` that's SVG/PHP, zip-slip, zip-bomb 100:1) · (7) CORS: victim origin reflected? credentials? · (8) login: rate limit + timing (known vs unknown user) · (9) secret in the *shipped bundle* and in the API error responses · (10) session: revoke on logout + logout-everywhere works.
- **Quarterly**: non-developer (or a different dev) runs the 10 items + the break-glass drill + a restore test; a pen-test **on the surface that changed or annually at T1** — a pen-test report is a *snapshot*, not a state; the CI + staging loop is the state.
- **When a real vuln lands**: fix at the layer where the invariant lives (injection → parameterization, not more validation), **write the regression test for the exact vector before the fix is "done"** (a fixed vuln is a passed test, or it's a rumor), **assume the secret leaked → rotate** (conservative rotation is cheaper than "it probably didn't"), disclose with a timeline (users/regulators/vendors — the SOC2/your customers expect the *call*, not the press), postmortem: which invariant failed, which layer *should* have caught it early, what now lives in CI.

## The "don't" list (over-engineering, named)

Home-rolled crypto or auth (the ceiling of DIY is: Argon2id + vetted session lib + standard tokens) · per-request mTLS / per-request signing on a private network (TLS + per-request tokens already *is* the authz; mTLS adds an ops surface for the network boundary the network already owns) · WAF with 500 hand-written rules (a managed baseline WAF at the edge is fine as defense-in-depth; a custom-rule dump is tech debt that *hides* the real fixes) · per-tenant DIY column-encryption (use the managed KMS/envelope for the *data class* that requires it — PII/PCI — not "all tenants"); · five scanners blocking every PR (CI = secret + dep + default-SAST + a CSP collector; that's the set) · password-complexity theater · per-UX-action 2FA (per-*sensitive-action*: reset, 2FA change, pay, export-all) · honeypots on internal tools · "the security review" as a document (a threat-model note + the CI loop *is* the review; the PDF is the artifact of a process that didn't happen) · zero-day paranoia rituals (the cost of an unpatched *exposed surface* dominates the ritual; **shrink the public surface** — every public endpoint is a line item in the attack budget, and that audit is the highest-ROI "security" work on this list).

## Detailed coverage

Practical web-app security without over-engineering or lockout — risk-tiering first (so a static site doesn't get mTLS and a payments API doesn't get "we'll add auth later"), the OWASP top 10 as concrete implementation notes (XSS, CSRF, injection, SSRF, IDOR, upload abuse), auth/session done right (passwords, 2FA/passkeys, JWT vs session), a CSP that doesn't brick the site (report-only → enforce), rate limiting, dependency/supply-chain basics, and the explicit "do not over-engineer" list. Use when hardening any web app, reviewing auth/CSP/headers, or deciding how much security a system actually needs.
