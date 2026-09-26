---
name: git-github
description: Git and GitHub in practice — object model, reflog recovery, pickaxe/bisect, commit discipline, rebase vs merge, force-push rules, conflict handling, gh CLI workflows, branch protection, CI essentials, and common disaster recovery. Use for any git operation, PR/review work, or when a repo is in a bad state.
---

# Git & GitHub

## Model (30 seconds)

- Objects: **blob** (file content), **tree** (directory), **commit** (tree + parents + message), **ref** (name → commit or reflog). There is no "working copy" in the model — the index (staging) is a *prepared tree*.
- `git status` = worktree vs index vs HEAD. `git diff` = worktree vs index; `git diff HEAD` = worktree vs HEAD; `git diff A..B` = two commits; `git diff A...B` = the changes B makes relative to the merge-base (what "what did B add" means).
- **`git reflog` is the undo button.** Every HEAD move is logged: `git reflog`, `git checkout <sha>` or `git reset --hard <sha>` to recover anything "lost" (detached HEAD, bad reset, deleted branch → `git branch recovered <sha>`).

## Reading (before changing anything)

- `git log --oneline --graph --decorate -20` · `git log -p -- <path>` (history of one file) · `git log -S'needle'` (pickaxe: commits that added/removed the literal) · `-G'regex'` · `git log --follow <file>` (through renames) · `git show <sha>` · `git blame -L <n>,<m> <file>` (useful for "who and why" when the message is thin: read the commit the blame points at).
- `git bisect run ./script.sh` — automate bisection (script exits 1 on bad). For a config flip, bisect over *values*.

## Working rules

- **Commit small and often, locally.** Unwinding 90-minute commits is how Fridays die.
- Message: `type(scope): subject` ≤ 72 chars, imperative ("fix", not "fixed"/"fixes"); body = *why* the diff can't say + issue ref; `BREAKING CHANGE:` footer for the big ones.
- **Only rebase your own unpublished commits.** `git rebase -i` → `reword` messages, `drop` junk, `fixup` (pair with `--autosquash`: commit with `git commit --fixup=<sha>`), `squash` to merge. Never rebase after someone has built on your commits.
- **`git push --force-with-lease`**, never `--force` (lease fails when someone pushed in between). On main/protected: never.
- **Merge vs rebase:** rebase keeps your branch history linear (pre-merge); `git merge` (or `--no-ff` merge on main) preserves the branch's existence + integrates. Team convention wins; apply one per repo and be consistent.
- Stash: **last resort**, WIP commit on a branch (or a `git worktree` for parallel work) beats `git stash` (stash is invisible, forgettable, misapplies across branches).
- Tags: annotated for releases (`git tag -a v1.2.3 -m "…"` + `git push --tags`); lightweight tags for scratch.
- `git clean -fdn` (dry run!) before any clean; `git gc --prune=now` only when you understand reflog + object retention.

## Conflicts

- `git rebase`/`merge` → resolve markers, `git add`, `git rebase --continue` / `git commit`. **Read both sides' intent** — deleting one side's line because "mine is cleaner" is the classic silent corruption. `git diff --diff3` shows the common ancestor.
- Long rebase: `git rebase --abort` is always safe; `git rebase --skip` only if you know the commit's content is already present (verify: `git cherry`).
- Renames + edits conflict: check `git status` for unmerged entries (both modified / added by both) and resolve each, then `git add`.

## GitHub / gh CLI

- `gh auth status` first. Day-to-day:
  - `gh pr create --fill` (auto-drafts title/body from commits), `gh pr view -w` (checks + timeline), `gh pr checks` (CI status, `--watch`), `gh pr diff`, `gh pr merge --rebase --auto`
  - `gh run list --branch X`, `gh run watch <id>`, `gh run view <id> --log-failed` (CI failure triage: start at the failed step's log, read the *last* error, not the first warning)
  - `gh issue list --label bug`, `gh api repos/.../commits` (raw data when the CLI lacks the shape), `gh api --paginate` for lists.
- **Review etiquette:** comment on behavior/invariants (see `software-engineering-wisdom`); `REQUEST CHANGES` for blockers only, `APPROVE` when you'd merge your own version; inline > general.
- **Branch protection on main:** require PR + review + passing status checks; `CODEOWNERS` (per-path `@team`) to route the right reviewer — the rule with no owner is the rule that breaks.
- CI: `on: pull_request` + `on: push` to main; cache deps (`actions/setup-node` + `actions/cache` or built-in); `concurrency: {group: refs/pull/$number, cancel-in-progress: true}` stops zombie builds; secrets in repo/org settings, never in workflow files; filter (path) so unrelated PRs don't wait on the full suite.
- **Release:** tag → GitHub release (or a release workflow); update `CHANGELOG` from the PR list, not memory.

## Disaster recovery playbook

- **Lost commits (bad reset):** `git reflog` → find the sha → `git checkout -b recovery <sha>` (keep the old branch, don't overwrite).
- **Deleted branch with work:** reflog of any commit on it, or `git fsck --lost-found` for fully orphaned objects.
- **Amended a pushed commit** (you already should not): push fails; `--force-with-lease` if you own the branch alone; otherwise re-create the original commit on a new branch and re-apply.
- **Brought main into a feature branch and now merge hell:** `git rebase --onto main <merge> feature` (move feature's commits onto main, skipping the merge commit). Verify with `git log --oneline main..feature` (only *your* commits should be listed).
- **Submodules:** prefer not (they are a support burden); when present, `git submodule update --init --recursive`; commit the *sha*, never the submodule's dirty state; `--force` to override a stray local detach.

## Housekeeping

- `.gitignore`: ignore build outputs, secrets, local config; **test it** (`git check-ignore -v path`) — a missing ignore rule is how `.env` ships.
- `.gitattributes`: `* text=auto eol=lf` (or per-platform), `*.png binary`, linguist-ignore for generated dirs.
- `git config pull.rebase true` (local default: rebase your pulls) — set intentionally, document in onboarding, don't surprise.
- `git config core.hooksPath` → shared pre-commit checks (or use a real hook framework) for lint/test gates pre-push.