# Changelog and release note patterns

## Keep-a-Changelog skeleton

```markdown
# Changelog
All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Export queue handles concurrent writes (#412)

## [1.4.1] - 2026-08-30
### Fixed
- `--json` no longer drops the final record on Windows (#408)
### Security
- Bump `undici` to 7.5.1 for CVE-2026-xxxx

[Unreleased]: https://github.com/org/repo/compare/v1.4.1...HEAD
[1.4.1]: https://github.com/org/repo/compare/v1.4.0...v1.4.1
```

## Generating grouped notes

```yaml
# .github/release.yml
changelog:
  exclude:
    authors: [dependabot[bot]]
  categories:
    - title: Breaking changes
      labels: [breaking]
    - title: Features
      labels: [enhancement, feature]
    - title: Fixes
      labels: [bug, fix]
    - title: Documentation
      labels: [documentation]
    - title: Other changes
      labels: ["*"]
```

Label discipline is what makes this useful; if the tracker has no labels, group by hand instead of shipping raw commit titles.

## Migration notes

Write them as an instruction the reader can execute:

1. What changed, in one sentence.
2. Who is affected (which configuration, API, or runtime).
3. The old code, the new code, and the intermediate state if a staged migration is required.
4. The removal timeline for deprecated paths.

## Before / after

**Before (raw log dump):**
> - fix: handle null in parser
> - refactor client
> - chore: bump deps
> - BREAKING: remove legacy mode

**After:**
> ### Breaking changes
> - `legacyMode` was removed. Use `mode: "compact"` — the wire format is identical (#401).
> ### Fixed
> - The CSV parser no longer returns `null` for empty trailing fields (#404).
> ### Notes
> - Dependency bumps only; no action required.

## Release checklist

- [ ] Change set read from merges/commits since the last tag, not from memory.
- [ ] Version bump matches the most severe change in the release.
- [ ] Breaking changes list the affected surface and the replacement.
- [ ] Every entry links to a PR/issue where the repository uses them.
- [ ] `CHANGELOG.md` updated (Unreleased section rolled into the new version).
- [ ] Tag points at the intended commit; prerelease flag matches the version shape.
- [ ] Artifacts, checksums and provenance attached where binaries are shipped.
- [ ] Published notes re-read once as a user: is it clear whether I must act?
