---
name: systems-security
description: Secure infrastructure using threat modeling, least privilege, secrets, network controls, supply-chain checks and incident response. Use for server hardening and operational security.
---


# Systems Security (operational)

## The posture (in one paragraph)

Security in ops is **least privilege + egress control + auditable state + tested recovery**. Most infra incidents are: a shared credential with too much scope, an egress path nobody knew about, a log that doesn't exist when you need it, and a backup nobody has restored. Control those four and you're ahead of most systems; the exotic stuff (mTLS mesh, per-pod attestation) is where teams spend the budget and lose the incident.

## Threat model in 10 minutes (do this before adding controls)

1. **Assets**: what's it bad if it leaks/is corrupted/is unavailable? Rank: (a) user credentials, (b) user data (PII/money), (c) secrets (keys, tokens), (d) availability.
2. **Trusted/untrusted boundary**: who/what can reach each asset (internet, vpc, other tenants, your deploys, a supply-chain dep, a *user's* data — user data is untrusted input even "inside" the system).
3. **One sentence per path**: "if X is compromised, the worst that happens is Y, and it's detectable by Z." If any sentence has no Z, that's your first control (detection), not your fifth.
4. **The three "who" questions**: who can deploy (code → prod), who can reach the DB (network + credentials), who can read a secret (who, *when*, and who *after* — ex-employees' tokens, a stale role, a shared key in a CI log).
- Output: one page, re-run on every major change (a new public endpoint, a new tenant model, a new dependency class) — the threat model is a *living half-page*, not a 40-page document (see `web-security`'s anti-theater note; same rule).

## Least privilege (the highest-ROI control, the least-done)

- **Identity per thing**: per-service identity (role) for the service, per-human identity for humans, **no shared root** (a root in a deploy script = every deploy = a root; the deploy *role* gets exactly the verbs it uses). Per-tenant scoping where multi-tenant (a tenant's data plane must not be reachable from another tenant's path — the `databases`/`api-design` object-level rule at the infra layer).
- **Credentials**: short-lived (minutes–hours, auto-rotated) over long-lived (a secret that lives 3 years in an env var is a leak waiting for the right grep); **break-glass** = the only long-lived, and it's **alerted on use** (an unused break-glass that's used = page someone) and has a documented owner.
- **DB**: per-app role (the app role can't `DROP`, can't read other tenants' schemas where separable); a separate *read-only* role for analytics; the **DB superuser is not in any app, CI, or notebook** (and if it is in a `.pgpass` on a laptop, it's a finding today, not in the next audit).
- **Cloud identity**: roles, not keys, for machines (IMDSv2/federation) — **IMDSv2 required** (v1 is the metadata-endpoint SSRF → instance-role → full compromise; see `web-security` SSRF), per-resource policies, and a **deny-by-default egress** (below).
- The audit question for every credential: **who requested it, what's its scope, when does it expire, and who verifies it's still needed?** If any of the four is "nobody/unknown", it gets a ticket with a date (a credential without an owner is a find, not an asset).

## Secrets lifecycle (not "set a secret", a lifecycle)

- **Store**: a vault/KMS (cloud KMS, Vault, sops for git-adjacent) — **never a repo, never a config file on a box, never a CI variable that's `visible in all jobs`** (a CI secret exposed to 40 jobs = 40 leak paths).
- **Access**: who (the service identity, not a human), when (at runtime, injected, short-lived where the platform allows), and **log every access** (a secret accessed by a role at 4am that never accesses at 4am is an event).
- **Rotation**: on schedule for the *class* (a credential that's been exposed in a log = rotate now; a 90-day rotation for a 3-year-old key = the finding), and **rotation is tested** (a rotation that breaks 3 integrations at midnight is a rotation finding).
- **In DR**: the **secrets are in the backup/restore path** (a DR drill that restores the DB but can't get the KMS key / DB password / signing key restored = "we have a database image"); include the key-identity and the *access* to it in the restore runbook and **run it** (quarterly, see Incident/DR below).
- **Exfil prevention**: secrets in **logs is the #1 real leak** (a `console.log(res)` with the token, a stack trace with the connection string, a CI log with a `--token=` flag): redact at the logging layer (a mask for the patterns), and a **log-scan in CI** (grep the *deployed* logs, not just the code, for the patterns + the actual secret values where you have them — a "secret-in-logs" alert on the live stream beats a quarterly code scan).

## Network posture (default-deny, egress first)

- **Ingress**: only the public surface is public (the API GW, the CDN origin, the LB) — **DB/WL/queue/admin = private, no public IP, ever** (a public DB = the #1 "we were breached and nobody noticed" in the small-company corpus); every public endpoint is a line item (see `web-security`'s surface-shrink note).
- **Egress is the modern perimeter**: data exfil and C2 leave, not enter. **Per-service egress allowlist** (what each service may reach — the web tier reaches the DB + the cache + the 2nd API, not "the internet"); a new egress path = a change event (alert on first-allow for a service reaching a new domain). The **cloud metadata endpoint is on the deny-list for every workload that fetches user URLs** (the SSRF→metadata RCE, again — it's the load-bearing control).
- **Segmentation**: the prod network is not the dev network (a dev box with prod creds is a one-key-stroke breach of "prod"), the data plane is not the control plane (where the multi-tenant split lives, `api-design`/`databases`), and the blast radius of a compromised *non-critical* service does not include the creds to the critical one (least-privilege identity, above, is the segmentation that actually holds — network rules are the second layer, not the claim).
- **TLS internally at T1** (in-transit between your own services: the "it's a private VPC, so" era ended with the insidere's laptop and the compromised dep; mutual TLS is *not* required (see the anti-override list) — mTLS is the *identity* story, and an identity per service + a private network + the allowlist covers 95% of the internal-transit risk without the mesh ops).

## Key management & data at rest

- **Use the managed KMS** (envelope: data-key wrapped by a KMS key, the data-key does the AES-GCM) — **a home-rolled key store is the over-engineering that loses** (the KMS gives you rotation, the audit, the HSM backing, and the "who accessed it" log; yours gives you a 2am you).
- **Encryption at rest is a data-class control, not a blanket one**: encrypt the *class* that requires it (PII, PCI, the thing the law/contract names); "encrypt everything" is a cost + a performance tax with a false sense (the *keys* and the *access* are the control, the ciphertext on an EBS volume you're root on is not the boundary — the boundary is the identity + the egress). Column-level for the named PII fields at T1 multi-tenant (where a tenant's rows live in a shared table), volume/database-level for the rest.
- **Backups & DR** (security-flavored): the backup is **encrypted with a key that's *in* the restore path** (above), the backup is **out of the blast radius** (a backup in the same region/vpc as the thing it backs up is a shared failure), the **restore is tested quarterly** (the test is the restore, not the "backup succeeded" green check), and **PITR to "before the incident"** is the named recovery for the "a bad deploy at 10:00 corrupted data by 10:05" case (you need a *time* you can stand at).
- **Data lifecycle**: the data you don't have is the data that can't leak — **a retention + deletion path per data class** (a "we keep everything forever" = an ever-growing attack surface + a compliance problem + a cost; "delete on request" is a *product feature* at T1 with PII, and it needs a tested path, not a policy PDF).

## Supply chain (the build is the attack surface)

- **Pinned, locked, reproducible**: the lockfile committed, the toolchain versions pinned (node/python/rust + the *build* tool), `--require-hashes`/`npm ci` where you can — **a build that "works" with today's `latest` and a different one next week is not reproducible** (`databases`/`web-security` for the per-language specifics).
- **Audit the *scripts* that run at build time** (the `postinstall`/`setup.py` RCE, `web-security`): the build machine is a credential-rich target — run the build in an ephemeral, low-identity, **egress-allowlisted** environment (the build reaches the package index + 2 CDNs, not "the internet"), and the artifact is **verified** (a checksum/signature the deploy checks — `cosign` *when you publish an artifact someone else installs*; an internal image the deploy already verifies the digest of is covered by the digest, don't stack).
- **The dep the dep depends on** is the real supply chain: the SBOM (CycloneDX) is the map of what you actually ship (`web-security`); a vulnerability day with the SBOM is a 20-minute triage.
- **Provenance**: the image/container is built from the pinned source at the pinned commit, the digest is what runs in prod, and **a "prod is running a commit no one wrote" is a P1** (a drift alert: the running digest vs the git tag that should own it).

## Audit logging (the log you need *at 4am*)

- **What gets logged**: **auth** (login, MFA fail, token issued/revoked), **admin actions** (every one, with who), **data access to the named sensitive class** (PII read/modified — at least at the API, not the raw DB query, or you'll get 10k logs and no signal), **privilege changes** (a role grant = the event), **secret access** (above), **deploys** (who, what, where, digest).
- **How**: structured, **append-only** (the log writer can't delete — a mutable log is a log the attacker edits), **out of the system it's logging** (the app's own disk is not the audit store; the log goes to the store the app can't write to destructively), **indexed by tenant + actor** (the 4am query is "what did *this* actor touch in *this* tenant in the last hour"), and **retention per the data class** (the PII-adjacent log has the same retention as the PII; a forever-kept audit log is a forever-kept PII leak).
- **Alert on the pattern, not the raw line**: 5 MFA fails in 2 min = alert; a role grant outside a deploy window = alert; an admin acting outside their normal hours = alert (a threshold you set *before* the incident, from the baseline, not during it).

## Patching & the incident

- **Patching**: a known **critical on an *exposed* surface** is < 48 h (a day-zero on the public API is *hours*, and the patch is the vendor's, you just have to be able to ship it — **the ability to deploy fast is the patching control**); 90 days for the non-exposed, weekly for the rest. The "security patch Thursday" is a myth (a 0-day exists precisely because the patch isn't the defense yet). The **patch = the test** (a patch without a regression for the vuln it fixes is a re-incident; the `web-security` "vector test first" rule at the infra layer: a CVE on your dep gets a *test in CI* that the patched code passes, not just a version bump).
- **Incident response — the sequence is the control** (memorize it, it's short):
  1. **Contain first, understand later** (revoke the credential, freeze the deploy, isolate the host/tenant, egress-block the path — the goal is to stop the *ongoing* leak, not to explain it yet).
  2. **Preserve** (the logs, the memory/core where feasible, the *state* — the "fix it and look at it later" that wiped the disk is the 2nd incident).
  3. **Communicate** (the timeline out, the severity, the "we contain it / we don't know yet" — the *honest* "we don't know the scope yet" on hour 2 beats the confident wrong "it's contained" on hour 5; users/regulators expect the call, `web-security` for the disclosure).
  4. **Root-cause, blameless** (which invariant failed, which layer *should* have caught it early, what now lives in CI/the audit log/alerts — the `software-engineering-wisdom` postmortem format; the output is *controls*, not names).
  5. **Verify the fix is *enforced*** (a "fixed" credential that's still in 3 env vars and a notebook = the vuln, not the fix — the standard is the rotation + the grep + the *alert* on the pattern, all live).
- **The incident is the threat model's validation**: an incident the threat model didn't predict = the model got updated on a real schedule (that's the point of the half-page, `web-security`).

## The anti-over-engineering list (infra edition, named)

A **service mesh / per-pod mTLS** for a 5-service stack (identity-per-service + private net + egress allowlist + TLS-in-transit covers the risk; the mesh is the 6th–20th service's control, brought in when the surface needs it) · **a home-rolled key store** (the managed KMS is cheaper and has the audit + rotation + the "who accessed" log; the DIY wins only in a regulated air-gapped case the DIY is *required* by, and that case has a name) · **per-tenant-isolation-by-separate-database as the default** (it's the *data-class + the threat-model* control for the named high-risk tenant class, not "all tenants"; a blanket physical split is a cost + a recovery-surface expansion for the 95% that don't need it) · **WAF custom-rule dumps** (the managed baseline at the edge is fine as the non-app-depth layer; 500 hand rules are tech debt that hides the app fixes — the `web-security` rule) · **SBOM + provenance + sigstore on an internal artifact nobody installs** (the tooling is right for the *published* artifact; on the internal one, the pinned digest + the locked build is the control) · **a zero-detection ritual** (a SIEM with 500 rules and no 3am baseline = the rules are noise; 10 *tuned* alerts on the 4 signal classes (auth, privilege, secret, egress-allow) beat 500 generic ones — a threshold you set from the *baseline*, not the fear) · **a "security review" as the deliverable** (the 1-page threat model + the CI/audit loop + the tested DR *is* the review; the PDF is the artifact of a process that didn't run — the `web-security` rule, applied to infra).

## Detailed coverage

Operations-level security for a running system — threat-modeling in 10 minutes, least privilege (iam/roles, no shared root), secrets lifecycle (vault, rotation, DR), network posture (default-deny, egress allowlists, metadata endpoint), key management (KMS envelope vs DIY), supply chain (pin/audit/lockfiles), audit logging, patching & incident response (contain→preserve→communicate), backups that include secrets, and the explicit anti-over-engineering list for infra. Use when securing servers/infrastructure, doing an infra security review, or reacting to an incident.
