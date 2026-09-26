---
name: github-readme-authoring
description: Write or overhaul a GitHub README — project front pages, profile READMEs and monorepo landing pages — with real positioning, verified quickstarts, honest badges and screenshots. Use for README creation, rewrites and front-page polish; not for git operations, docs-site builds or general prose editing.
---

# GitHub README authoring

A README is a front page, not a manual: a stranger decides in about thirty seconds what the project is, who it is for and how to reach a first result. Optimize that decision before adding anything else.

Start from what proves what the project does — package manifest, real `--help` output, exported API, tests, CI — and describe the code that exists rather than the roadmap. Preserve the author's voice and structure; edit what is wrong or missing.

Order by usefulness: name, one-line purpose, then only what applies — screenshot or demo, install, the smallest copy-pasteable quickstart with expected output, a usage/configuration table, honest status and limitations, and links to contributing and license. Anything longer than roughly two screens belongs in `docs/`.

Verify before publishing:

- Run every documented command from a clean checkout; state required secrets, OS or paid services in the same block.
- Never put real tokens, internal hostnames or personal addresses in examples.
- Claims, badges and compatibility statements must trace to code or CI.
- Links, anchors and relative image paths must resolve on GitHub and on the package registry page.

Badges are evidence, not decoration: at most three to five, each reflecting state that actually updates. Keep headings, fences and tables consistent; ragged spacing is what makes a README read as machine-generated.

Read [README patterns](references/patterns.md) for skeletons by project type, badge and asset rules, weak-to-strong openings and the final review checklist.
