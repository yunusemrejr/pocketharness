---
name: harness-self-maintenance
description: Maintain the running Muse harness itself — user and project skills, the skills store and harness configuration — using explicit authorization, bounded discovery and focused verification.
---

# Harness self-maintenance

Use this workflow when changing the harness that runs the session: skills under `~/.config/muse/skills/` (user scope) or `.agents/skills/` (project scope), and harness configuration under `~/.config/muse/`. Discover the actual roots first; a similarly named checkout or another harness's directory (such as `~/.codex/` or `~/.pi/`) is not the live Muse harness.

Maintenance authority comes from the human's explicit request to change the harness itself, not from the working directory. Never treat an ordinary project task as authorization to mutate user-scope skills or harness configuration. If the request is ambiguous, confirm whether the change targets the harness or the current project before editing. Ordinary project-local edits must remain allowed; preserve protection of shared/global skills.

Inspect the skills store with `muse skills list` and `muse skills inspect` before changing it. Stage user-scope changes as drafts and run `muse skills validate <dir> --json`; install only through `muse skills install` so provenance stays recorded, and never hand-edit the store directly. Follow the skill's own references to find consumers and existing tests. Distinguish durable source, installed copies, generated assets and runtime state: patch the durable source and use its supported install/update path; store-only edits are lost on the next install or update.

Reproduce the behavior before changing it. Preserve credentials, active sessions and unrelated work. Verify the changed behavior and its failure path with focused checks: a re-run of `muse skills validate` plus the smallest behavior probe that exercises the change. For permission-sensitive changes, verify both the denied case and the allowed case. Read [verification and releases](references/verification-release.md) for process, permission or publication changes.

Record a dated maintenance note with rationale, coverage and any required restart or session reload. Separate store validation from live behavior; do not disrupt sessions owning unfinished work.

Check for a Git remote before publishing durable changes. Commit and push verified changes only when authorized. Never publish runtime data, session logs, private backups or credentials; review every export diff, keep third-party notices intact, and verify a fresh installation when the change affects one.
