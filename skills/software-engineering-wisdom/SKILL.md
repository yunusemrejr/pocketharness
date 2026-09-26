---
name: software-engineering-wisdom
description: Make engineering tradeoffs, architecture decisions, code reviews and delivery plans. Use for maintainability, dependency choices, incidents and deciding what is worth building.
---


# Professional Software Engineering

## Decision-making

- **Name the tradeoff explicitly** for every non-trivial choice: "X over Y because Z; revisit when W." Unspoken tradeoffs are the source of all architecture fights.
- **Two-way doors** (cheap to reverse — config, feature flag, new module): decide fast, ship, measure. **One-way doors** (data model migration, chosen DB, public API, vendor lock-in): slow down, write it down, get a second reader.
- **Write decision records (ADR) for one-way doors**, 5–15 lines: context, options, decision, consequence. The goal is that a future reader knows *why*, not that the doc is formal.
- **Default to boring.** New technology tax = learning + docs + hiring + tooling. You're paid for outcomes; a new framework is only justified by a concrete, existing problem it solves that the incumbent can't.
- **Boring ≠ frozen:** boring is the default, not a law. When the pain is real and measured (p95 latency, 3-week refactor for a 2-week-old feature), change deliberately and say so.
- **Reversibility ranking** when torn: prefer options that leave both doors open (new path + old path, feature-flagged) until the path is proven.

## Code & review

- **Read before write.** Understand the existing owner of the concern (cache, retry, auth, config) before adding a parallel one. Extend; don't fork.
- **Deletion is a feature.** When one path can do the work of two, the second path is a maintenance liability, not flexibility.
- **Small PRs** (roughly < 400 lines of diff): reviewable in one sitting, bisectable when they break, less rework. Split by concern, not by effort.
- **The commit message is the changelog entry:** what + why in ≤ 3 lines, issue refs, "fixes #N" for merge-close. Body for the *why* the diff can't say.
- **Review etiquette:** comment on behavior and invariants, not style (lint owns style). Praise specific good choices — it calibrates the author. Disagree on the *problem* first, not the solution. "Is this the right layer?" before "I don't like this naming."
- **You review your own work with the author's hat off:** ask "what would break this at 3am?" — concurrency, partial failure, retries, empty input, the 10th concurrent write.

## Trust boundaries & safety

- Validate at **every** boundary (HTTP input, IPC, file parse, config) — "internal" code is still external until proven otherwise.
- Fail loud, not quiet: a wrong default choice (auth disabled, cache shared across tenants) silently shipped is p0-grade.
- **Least privilege by default:** tokens scoped, DB role-read-only for the reader, secrets in the vault not the repo, egress allowlisted for services.
- Irreversible operations (delete, deploy, payment) get confirmation/audit/dry-run; everything else gets idempotency keys so retries are safe.

## Incidents & quality bar

- **Fix forward, then ask why.** Stop the bleed first; root-cause after, blameless: the system allowed it, not "someone was careless."
- Postmortem format: timeline (facts, with timestamps), impact (who, how many, how long), cause (the 5-why chain to the system flaw), action items (each with owner + date). Vague items ("improve monitoring") die; specific ones ("alert when queue depth > 1000 for 5 min") ship.
- **SLOs over vibes:** define the thing users feel (p95 < 400ms, 99.95% success) and measure it; error budget = when to stop shipping features to stabilize.
- **Latency is p95/p99, never average.** Dashesboards on averages hide the pain.

## Dependencies & tooling

- Every dependency is permanent: someone's security, license, and maintenance risk in your tree. Ask: actively maintained? license compatible? smaller alternative that's honestly good enough? (stdlib — see `ponytail`.)
- **Own what's critical.** The thing your product is *known for* (the scheduler, the diff engine) — you probably should write it. The commodity (date parsing, HTTP client) — buy it.
- Lockfiles committed, versions pinned for deploys, upgrade window regular (an afternoon per month), not a big-bang yearly.

## Communication & scope

- **Ask when ambiguity changes the action** (deploy? delete data? touch prod?); otherwise proceed and document the assumption.
- Bad news early, with the fix attached: "The DB is 90% full; here's the cleanup I ran + the alert I added" beats "oops" on Friday.
- Estimate in **classes** (hours / days / weeks) and say what would break the estimate; refine, don't re-zero, when new information lands.
- "Working software over documentation" — but a 20-line design note before a 2000-line change saves the week. The bar is *disagreement surface*: what can be argued about pre-flight?

## The habit stack

Reproduce the bug before fixing it · one-variable changes · tests around the fix, not around the symptom · ask "what's the smallest thing that proves this works" · measure before optimizing · delete the scaffolding after · say what's untested instead of implying it is.

## Detailed coverage

Senior-engineering judgment — decisions, tradeoffs, communication, code review, incidents, dependencies, and shipping discipline. Use when reviewing work, making design/architecture calls, writing PRs or docs, or judging whether something should be built at all. Pairs with ponytail (build lazily) and evidence-first-engineering (verify against reality).
