# Branching and review

## Branch models

Trunk-based development (short-lived branches off main, merged within days) suits teams with strong CI and feature flags: integration is continuous and releases cut from main at any green commit. Long-lived release branches suit scheduled versions with stabilization windows: main moves forward while the release branch accepts only fixes. GitFlow-style multi-tier branching suits few teams today — adopt it only when the release train genuinely needs parallel stabilization, and document who merges where.

Whatever the model, write it down: branch names, lifetimes, merge direction, and what each branch promises (main is always releasable; release branches accept only defined fix classes). An undocumented model is no model — developers will invent five incompatible ones.

## Pull-request policy

Keep changes small and single-purpose: a reviewable unit with a clear description, linked issue, test evidence, and rollback notes. Large diffs get shallow reviews; split by concern, not by file count. Require green checks before merge (build, tests, lint, security gates that the team actually maintains) and treat check failures as merge blocks, not advisory color.

Define review expectations: who must review (CODEOWNERS for sensitive paths), what reviewers owe (correctness, tests, readability — not style bikeshedding, which belongs to formatters), and response-time norms. Stale reviews kill trunk flow faster than any tooling gap. Automate the mechanical (formatting, trivial lint, changelog labels) so human review spends its budget on judgment.

## Merge strategy

Squash, rebase, or merge commits — pick one default per repository and document why. Squash keeps main linear and revertible per change; rebase preserves granular history for archaeology; merge commits preserve branch topology at the cost of noisier logs. Mixed strategies in one repository confuse bisecting and reverting; consistency beats theoretical purity.

Protect the main line: required checks, required reviews, no direct pushes, and no force-pushes to shared branches. Dismiss stale reviews on new pushes for sensitive paths. These protections live in the hosting platform, not in convention — verify them in settings, not in a wiki page.

## Monorepo versus multi-repo

One repository simplifies atomic cross-project changes, shared tooling, and unified CI; it demands build discipline (affected-only testing, clear ownership boundaries) as it grows. Many repositories isolate teams and release cadences; they demand versioning discipline (published interfaces, compatibility windows) for every cross-repo dependency.

Choose by coupling: code that changes together should live together. For monorepos, enforce ownership (CODEOWNERS, scoped CI) so growth does not dilute review. For multi-repos, version internal dependencies explicitly (tags, lockfiles) and test consumers against new versions before promoting them — floating references across repositories recreate monorepo coupling without monorepo atomicity.

## Versioning inside the flow

Tag releases immutably (`vMAJOR.MINOR.PATCH`) on the exact shipped commit; never move a release tag. Pre-release tags mark candidates, not moving quality gates. Keep a changelog derived from merged work (see github-release-notes) so the release content is auditable. Signing tags and artifacts belongs to the release ceremony — keys live in the OS environment or the CI secret store, never in the repository.

## Primary references

Check the documentation for the deployed version when behavior matters. These are reference entry points, not permission to change settings.

- https://trunkbaseddevelopment.com/
- https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches
- https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests
