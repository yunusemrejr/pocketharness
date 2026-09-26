---
name: google-identity-integration
description: "Implement Google sign-in and OAuth securely across web, native and backend applications."
---

# Google Identity Integration

Use for “sign in with Google/Gmail”, Google OIDC and delegated Google API access. Authentication and API authorization are distinct jobs.

## Working method

- Select the supported flow for web, SPA, native or backend clients.
- Validate identity tokens and correlate the login attempt.
- Key accounts by issuer and subject, with explicit account-linking policy.
- Minimize scopes and test failure, replay and session logout.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
