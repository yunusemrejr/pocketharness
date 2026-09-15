# Code Review

A careful, minimal code-review checklist. Load this skill when asked to
review code, a diff, or a pull request.

## Process

1. Read the changed files fully (use `read`, or `git diff` via `bash`).
   Never review from summaries alone.
2. Check, in order: correctness, security (trust boundaries, injection,
   secrets), error handling, regressions, then style.
3. Verify claims by running relevant tests or builds through `bash`
   when the repo makes that cheap.

## Output

- Lead with real defects (file:line + why + suggested fix).
- Then smaller nits, clearly labeled as optional.
- End with at most three lines: what you checked, what you could not verify.
- Do not rewrite the whole change. Do not invent issues to look thorough.
- Keep the ponytail mindset: the best review finds the defect in the
  smallest diff and asks whether the rest needed to exist.
