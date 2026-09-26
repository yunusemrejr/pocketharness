---
name: github-actions-workflows
description: Write, review or harden GitHub Actions workflows — triggers, matrix and caching, least-privilege permissions, pinned actions, concurrency, secrets vs OIDC, required status checks and CI failure triage. Use when authoring or fixing CI/CD pipelines or workflow YAML; not for local build debugging, generic shell scripts or git history work.
---

# GitHub Actions workflows

Build the pipeline from the checks a developer already runs, with pinned tool versions; CI should invoke the same commands rather than a parallel implementation.

- **Permissions and supply chain**: `permissions: contents: read` at workflow level with write scopes only on the job that needs them; pin third-party actions to a commit SHA with the version in a comment; never combine `pull_request_target` with an untrusted checkout; pass `github.event.*` values through `env:` instead of interpolating them into `run:`.
- **Triggers, cost, caching**: `pull_request` plus `push` to the default branch, `paths` filters, `workflow_dispatch`; `concurrency` with `cancel-in-progress` for pull requests, never for releases; cache on lockfile hash, OS and runtime; set `timeout-minutes`.
- **Secrets and deploys**: repository, organization or environment secrets, never echoed or written into artifacts; prefer OIDC short-lived credentials over stored cloud keys; gate irreversible deploys with environments and reviewers; fork builds never receive secrets, so design around that.
- **Required checks**: branch protection matches job names, so a rename silently un-requires a check; prefer one aggregating job over five fragile ones.
- **Triage**: `gh run view <id> --log-failed`, then the first failure inside the step; separate runner, cache and registry incidents from code failures; never mask a gate with `continue-on-error` or `|| true`.

Read [workflow patterns](references/patterns.md) for CI skeletons, release provenance, OIDC deploys, the hardening checklist and common failure signatures.
