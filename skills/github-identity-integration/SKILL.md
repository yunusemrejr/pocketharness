---
name: github-identity-integration
description: "Implement GitHub user sign-in with OAuth or GitHub Apps and explicit account linking."
---

# GitHub Identity Integration

Use for application login with GitHub. Distinguish user identity from repository installation permissions and automation credentials.

## Working method

- Choose OAuth App versus GitHub App based on the actual permissions needed.
- Bind authorization responses to one login attempt and exchange codes server-side where appropriate.
- Resolve the authenticated user through GitHub and use a stable provider ID.
- Verify minimal scopes, session handling and denied/replayed flows.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
