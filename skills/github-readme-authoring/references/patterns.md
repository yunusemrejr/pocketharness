# README patterns

## Skeletons by project type

**Library / package.** Name + one-line purpose → install → import + smallest call → one realistic example with output → API summary table or link → compatibility (runtimes, versions) → contributing/license. Status line if pre-1.0.

**CLI.** Name + one-line purpose → install (package manager, binary release, container) → `--help` excerpt → three commands covering the common paths → output sample → configuration/flags table → exit codes and files it writes → license.

**Application / service.** Name + what it does for whom → screenshot or demo → hosted option or local run (`docker run …`, `npm run dev`) → required environment variables table → deployment note → limitations/status → license.

**Monorepo.** Name + scope → package table (`path | purpose | status`) → prerequisites → install and run one package → how checks run across the repo → conventions (changesets, release flow) → license.

**Internal tool.** Name + owner/team → what problem it solves → how to run it locally → how to deploy and roll back → who to contact. Keep it short; internal readers need the runbook, not marketing.

**Template / starter.** What you get → "Use this template" button or `degit`/`git clone` → first-run steps → what to rename or configure → what is intentionally not included → license.

## Badge reference

| Fact | Type | Notes |
| --- | --- | --- |
| CI status | dynamic | Must point at the real workflow; shows red when CI is broken, which is honest |
| Latest release / tag | dynamic | Release badge, not a hand-edited version string |
| License | static | Matches the LICENSE file exactly (SPDX id) |
| Runtime / platform support | static | Only the versions CI actually exercises |
| Package downloads | dynamic | Meaningful for published packages; noise for internal repos |
| Coverage | dynamic | Only when a real, current report is published; never a hardcoded percentage |

## Asset rules

- Path: `docs/assets/` (project docs) or `.github/assets/` (repo-only). Lowercase, hyphenated, descriptive: `export-queue-dark.png`.
- Screenshots: 900–1400 px wide, cropped to content, no personal data, no real customer names or tokens visible.
- Animated demos: under ~5 MB, loop one interaction, avoid a long intro delay before the first visible action.
- Every image gets alt text describing its content; decorative separators get empty alt text.
- Reference assets with relative paths that also resolve on the package registry page when possible.

## Openings: weak → strong

| Weak | Why | Strong |
| --- | --- | --- |
| "Welcome to ProjectX!" | Says nothing; the name is already the heading | "ProjectX indexes your local photo library and finds near-duplicates." |
| "A fast, modern, powerful tool." | Adjectival claims no one can verify | "Converts 10k-row CSVs to Parquet in one pass, 40 MB RSS." |
| "This repository contains the source code." | Restates the obvious | "Self-hosted release tracker; runs on one Node process with SQLite." |
| "Built with ❤️ by the community." | Social filler before value | Move to Acknowledgements, at the bottom. |

## Final review checklist

- [ ] Top third answers: what is it, who is it for, how do I start.
- [ ] Every command was executed from a clean checkout in the last change.
- [ ] Relative links, image paths and badge URLs resolve; anchors match headings.
- [ ] No secrets, private hostnames, personal data or internal issue links.
- [ ] Claims trace to code, CI or a published release; limitations are stated.
- [ ] Badges ≤ 5 and each one reflects real state.
- [ ] Images have alt text and are stored under a documented asset path.
- [ ] Contributing/license/security links point at files that exist.
- [ ] Rendered preview checked on GitHub (light and dark) at a narrow width.

## Primary references

README structure and badge conventions follow <https://www.makeareadme.com/> and <https://shields.io/>; Keep a Changelog and SemVer are linked from the release workflow.
