# Community health files

## CONTRIBUTING.md

Start with prerequisites and the exact commands:

```markdown
## Development
Requirements: Node >= 22, pnpm 9.
    pnpm install
    pnpm test
    pnpm lint
## Proposing a change
Open an issue for behavior changes before writing code. Keep pull requests focused;
one concern per PR. Run the checks above and paste the results in the description.
```

Include: review expectations and typical latency, coding or commit conventions that reviewers actually enforce, how to run a single test, and whether contributions to docs, translations or examples are wanted. Do not write aspirational process that reviewers do not follow.

## SECURITY.md

```markdown
## Supported versions
| Version | Supported |
| 1.4.x   | yes       |
| < 1.4   | no        |
## Reporting
Report vulnerabilities through GitHub's private advisory form on this repository
(Security → Report a vulnerability). Do not open a public issue.
Include: affected version, impact, reproduction, and any suggested fix.
We acknowledge reports within three working days.
```

Never publish a personal email you are not prepared to monitor, and never promise an SLA the project cannot meet.

## CODE_OF_CONDUCT.md

Adopt an established text (for example Contributor Covenant) and fill in the enforcement contact. The contact must be a private, monitored channel. State the scope (issues, PRs, discussions, chat, in-person events) and the enforcement ladder rather than "we will handle it".

## Issue and PR templates

- `bug_report.yml`: fields for version/commit, environment, steps, expected vs actual, logs. Mark the version and reproduction fields required.
- `feature_request.yml`: problem statement first, then the proposed approach; ask for alternatives considered.
- `config.yml`: `blank_issues_enabled: false` only when the templates cover real cases; add `contact_links` for questions (discussions) and security (policy page).
- `PULL_REQUEST_TEMPLATE.md`: what/why/verification + a checklist that includes "tests run", "docs updated", and breaking-change notes.

## CODEOWNERS

```
# .github/CODEOWNERS
/src/api/        @org/api-team
/docs/           @org/docs-team
*.tf             @org/infra
```

Rules are last-match-wins. Verify paths exist, use teams rather than individuals where possible, and re-check after large directory moves.

## Settings checklist

- [ ] Description, topics, homepage and social preview set and accurate.
- [ ] LICENSE detected; third-party notices present for vendored code.
- [ ] CONTRIBUTING / SECURITY / CODE_OF_CONDUCT / SUPPORT present where they apply, with working links.
- [ ] Issue templates + `config.yml` route questions and vulnerabilities away from the tracker.
- [ ] PR template asks for verification evidence.
- [ ] CODEOWNERS patterns match existing paths and resolve to real users/teams.
- [ ] Default branch protected; required checks match existing job names.
- [ ] Auto-delete head branches, Dependabot security updates and private vulnerability reporting enabled.
- [ ] Deprecated repositories archived with a pointer to the replacement.

## Primary references

Use the canonical community texts rather than rewriting them: <https://www.contributor-covenant.org/version/2/1/code_of_conduct/>, <https://choosealicense.com/>, and GitHub's own guidance at <https://docs.github.com/communities/setting-up-your-project-for-healthy-contributions>.
