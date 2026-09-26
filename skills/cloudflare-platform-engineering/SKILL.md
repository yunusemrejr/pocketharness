---
name: cloudflare-platform-engineering
description: "Integrate Cloudflare Workers, caching, storage, APIs and server-validated Turnstile."
---

# Cloudflare Platform Engineering

Use for Cloudflare application integrations and deployments. Confirm the selected product, account scope and compatibility date before changing behavior.

## Working method

- Map request flow through browser, edge and origin.
- Keep secrets server-side and API tokens narrowly scoped.
- Specify cache keys, authorization boundaries and consistency requirements.
- Validate local behavior and the actual deployment separately.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
