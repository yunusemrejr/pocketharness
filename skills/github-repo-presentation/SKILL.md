---
name: github-repo-presentation
description: Prepare a repository's public front door — description and topics, license, CONTRIBUTING, SECURITY, code of conduct, issue and PR templates, CODEOWNERS, social preview and Pages. Use when opening a repo to outside contributors or auditing its first-run experience; not for code changes or README prose alone.
---

# GitHub repository presentation

The public surface is small files and settings that decide whether a stranger can evaluate, run and contribute. Set them deliberately: a `CONTRIBUTING.md` link with no real process, or a security policy with no reporting channel, is worse than none.

- **Metadata**: a one-line description naming what it is and who it is for, 5–10 real topics, a 1280×640 social preview, a homepage that resolves.
- **License**: choose it with the copyright holder and ship the full text as `LICENSE`; never copy a license carrying another project's name, and keep notices for vendored code.
- **Health files** in `.github/`: contributing (the build, test and review steps that work), security (supported versions, private reporting channel, expected response), code of conduct (standard text plus a monitored private contact), and support or citation where real.
- **Templates**: bug reports asking for version, environment, reproduction and expected vs actual; feature requests starting from the problem; a PR template requiring verification evidence; `CODEOWNERS` patterns that match real paths.
- **Settings**: protect the default branch with checks whose job names exist, delete head branches automatically, enable Dependabot security updates and private vulnerability reporting, and archive deprecated repositories.

Verify from outside: open the repository logged out, check the About panel and social card, open the new-issue and new-PR pages, confirm license detection and the security path, then follow `CONTRIBUTING.md` in a fresh clone.

Read [patterns](references/patterns.md) for per-file starting points, enforcement contacts and the settings checklist.
