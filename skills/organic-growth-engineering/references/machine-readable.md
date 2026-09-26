# Machine-readable publishing without duplicate truth

Serve the meaningful article or product description in initial semantic HTML where practical: `main`, `article`, ordered headings, lists, tables, descriptive links and timestamps. Keep navigation separate. Use canonical URLs and accurate metadata; add JSON-LD only for applicable facts also present to readers. Validate output against the page, not only a schema parser.

Offer RSS/Atom or a documented JSON feed for changing publications when useful. For substantial documentation or agent-heavy audiences, generate concise Markdown alternatives from the same content source. Preserve titles, canonical URLs, author/source attribution, update dates, meaningful tables, code and limitations. Avoid hidden extra promises or instructions that try to influence an agent's recommendation.

The [llms.txt proposal](https://llmstxt.org/) currently describes a compact Markdown guide linking to detailed agent-readable content. Treat it as an optional discovery aid, not a universal crawler contract. Its v2 guidance includes `rel="alternate" type="text/markdown"` for Markdown versions and `rel="describedby"` for the relevant `llms.txt`. Check the current proposal and target consumer before adopting version-specific conventions.

Example links, using public content only:

```html
<link rel="canonical" href="https://example.org/docs/install">
<link rel="alternate" type="text/markdown" href="/docs/install.md">
<link rel="describedby" href="/docs/llms.txt">
```

Keep a guide small and topic-organized so clients can retrieve detail only when needed. Do not generate a giant `llms-full.txt` by default. Repository `AGENTS.md` instructions serve coding workflows; publishing one on a website does not establish a standard search ranking mechanism.

Use compression, caching and conditional requests with content-derived ETags where supported. Explicit `.md` endpoints avoid user-agent-specific content drift. If using content negotiation, send correct content types and `Vary: Accept` and test CDN cache isolation.

Check text-only extraction, links, content parity, accessible media descriptions and representative response sizes. Access control must apply to every representation: a private HTML page must not gain a public Markdown or feed copy. `robots.txt` is crawler guidance, not authentication; training and search crawler controls require deliberate provider-specific policy decisions.

[Google's AI-feature documentation](https://developers.google.com/search/docs/appearance/ai-features) requires no special AI text file or markup. Measure retrieval usability independently from search traffic; neither a valid guide nor good schema guarantees citations.
