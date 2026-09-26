---
name: php-application-engineering
description: "Build secure PHP applications across local development, FPM and shared cPanel-style hosting."
---

# PHP Application Engineering

Use for PHP application code and deployment-sensitive PHP debugging. Follow the installed PHP version, extensions and framework conventions; do not assume CLI PHP and the web SAPI share configuration.

## Working method

- Inspect composer constraints, PHP/SAPI versions, extensions and document root.
- Separate request parsing, authorization, domain logic, persistence and rendering.
- Validate boundaries, escape output by context and parameterize database values.
- Test local behavior and hosting-specific filesystem/session/cron constraints.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
