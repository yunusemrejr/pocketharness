---
name: multi-developer-pipelines
description: "Run version control for busy teams and complex deployments: branching models, trunk-based flow, pull-request and review policy, monorepo versus multi-repo, environment promotion, feature flags, hotfix and rollback discipline, and pipeline observability."
---

# Multi-Developer Pipelines

Use when several developers share a codebase with staged deployments — branching strategy, review policy, release flow, environment promotion, or rollback planning. For Git mechanics, history repair, and day-to-day GitHub work use git-github; for authoring CI workflow YAML use github-actions-workflows; for writing release notes use github-release-notes; for repository health files use github-repo-presentation.

## Working method

- Make the flow visible before changing it: map branches, environments, gates, and who can promote what. Most pipeline pain is undocumented flow, not missing tooling.
- Keep the main line releasable: short-lived branches, small reviewable changes, required checks that actually gate merges, and a merge strategy the team understands. Long-lived divergent branches are deferred merge conflicts with interest.
- Separate deploy from release: code reaches environments through promotion, features reach users through flags and rollout policy. Conflating the two turns every deploy into a release decision under pressure.
- Plan the way back first: every promotion path needs a rollback path with the same ceremony as the forward path. An untested rollback is a hope, not a plan.

Read [branching and review](references/branching-and-review.md) for branch models, pull-request policy, and repository layout. Read [deployment and rollback](references/deployment-and-rollback.md) for promotion, flags, hotfix flow, and observability; do not load it for pure branching questions. User instructions take precedence; this skill adds no authority to change branch protection, merge code, or deploy.

## Evidence and completion

Report the flow as found, the change proposed with its blast radius, and how it was validated (dry run, staging promotion, or documented review). Name the rollback path and any untested steps. Do not present an unreviewed flow change as team agreement.
