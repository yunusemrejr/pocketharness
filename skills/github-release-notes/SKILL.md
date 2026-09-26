---
name: github-release-notes
description: Cut a release and write its notes — semantic-version decisions, Keep-a-Changelog entries, GitHub Release bodies, prereleases, breaking-change and migration sections. Use for changelogs, release notes, version bumps and tag/release preparation; not for routine commit messages or git history recovery.
---

# Release notes and changelogs

Notes are written for people deciding whether to upgrade. Derive them from what merged rather than memory: `git log --oneline v1.4.1..HEAD`, `git diff --stat`, `gh pr list --state merged`, and the linked issues when a commit title hides a compatibility change.

Pick the version from the most severe change: removed or renamed public API, changed defaults or output format → major; new compatible capability or deprecation → minor; fixes and dependency patches → patch; security fixes ship as patches on each supported line. For `0.x`, state breaking changes explicitly.

Order sections by reader interest — upgrade and breaking changes first, then added, changed, fixed, removed, deprecated, security — saying what changed in behavior, who is affected, and linking the PR or issue. Include the before/after a reader needs:

```markdown
### Upgrade from 1.3
`client.run()` no longer retries by default; pass `retries: 3` to keep it.
```

Keep `CHANGELOG.md` current as work merges: `## [Unreleased]`, one dated section per release, compare links, and no rewriting of shipped history. Treat auto-generated notes as a starting point — configure `.github/release.yml` categories, then edit for impact and migration. Tag the exact commit you intend to ship, mark prereleases and "latest" deliberately, attach artifacts with checksums or provenance, and correct a published release instead of silently rewriting it.

Read [changelog and release note patterns](references/patterns.md) for the Keep-a-Changelog skeleton, categories, migration patterns and a before/after example.
