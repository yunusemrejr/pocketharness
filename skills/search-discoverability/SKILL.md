---
name: search-discoverability
description: "Implement technical SEO, crawlability and evidence-based visibility in search and AI answers."
---

# Search Discoverability Engineering

Use for indexable websites, metadata audits and content discovery. Ranking and chatbot citations are outcomes to measure, never guarantees. This skill complements page design; it does not prescribe a marketing campaign.

## Working method

- Establish canonical host, locales, public/private routes and the intended audience.
- Inspect HTTP status, rendered content, crawl controls and links before tweaking keywords.
- Make titles, headings, descriptions and structured data describe the actual page.
- Validate representative pages and measure indexed coverage and useful visits over time.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.

## Precise native audit

`pocket kit seo FILE|URL` checks retrieved source metadata, title/canonical syntax, robots indexing directives, image alt presence and JSON-LD object/array syntax. It ignores comments and script/style examples, accepts case/whitespace variation in real attributes, and requires a successful HTTP source fetch. Definite source issues determine failure; missing optional metadata, heading structure and mobile-rendering concerns are separate advisory context checks. Short pages and short/long titles or descriptions do not fail an arbitrary word/character quota.

Use `pocket kit net URL` for actual status, redirect/TLS/timing and response headers, then inspect `X-Robots-Tag`, canonical delivery and the authorized site's robots.txt/sitemap when those affect indexing. The source scanner does not establish rendered content, schema eligibility, search indexing, ranking or AI citations. A `noindex` page may be intentional; compare the observed directive with its public/private route contract before editing. A metadata-only audit does not require a visual redesign workflow.

Work representative public pages, private/error routes and duplicate URL variants before broad changes. Preserve content identity and real claims; one focused evidence pass is enough until the artifact or unresolved question changes. Deliver the implemented metadata/crawl fix with local/source, HTTP and available rendered evidence, then measure actual search coverage over time.

Primary guidance: [Google title links](https://developers.google.com/search/docs/appearance/title-link), [snippets and descriptions](https://developers.google.com/search/docs/appearance/snippet), and [Search essentials](https://developers.google.com/search/docs/essentials). These describe context-dependent search behavior; a local audit cannot guarantee it.
