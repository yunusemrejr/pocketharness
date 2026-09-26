# Search Discoverability Engineering: patterns and examples

## Crawl and document contract
Use one clear primary topic, a descriptive title, logical headings and real anchor links. A heading level expresses hierarchy, not font size; a second h1 is not a magical penalty, but confusing structure hurts readers. Put essential text and links in reliably rendered HTML; evaluate SSR/prerendering when a client-only app hides content. Use semantic lists, tables with headers, meaningful alt text, and stable URLs.

Return 200 for useful pages, actual 404/410 for removed resources, and deliberate permanent redirects for moved URLs. Avoid redirect chains, soft 404s and search/filter URL explosions. Keep canonical, hreflang alternates and sitemap URLs consistent. Canonical is a hint, not an access control. A sitemap lists canonical indexable URLs with truthful lastmod values; do not stamp every page as updated on every build.

Example head for a public article:
```html
<title>Calibrating an ESP32 temperature sensor | Lab Notes</title>
<meta name="description" content="A measured two-point calibration procedure, with test data and error limits.">
<link rel="canonical" href="https://example.com/lab/esp32-calibration/">
```
Replace the example domain. Open Graph/Twitter metadata helps sharing; it is not proof of search ranking. robots.txt controls crawling, not secrecy: blocking a URL can prevent a crawler from seeing its noindex directive. Keep private content behind authentication. Test accidental staging noindex, duplicated canonical tags and blocked JS/CSS before launch.

## Structured content and AI discovery
Use supported schema.org JSON-LD that matches visible facts. Never fabricate ratings, authors, availability or review counts. Publish primary evidence, definitions, dates, authorship when meaningful, explicit units, sources and concise answers inside useful context. Search engine AI features do not require a special AI schema; llms.txt is not an established Google ranking requirement. Implement it only for a concrete consumer or requested experiment, not as a universal SEO cure. Separate crawl permission from training permission according to each crawler's actual policy.

Measure search impressions, qualified visits, crawl/index errors and conversions; chatbot citations vary with query and time. Compare matched query sets, retain uncertainty, and avoid hidden text, keyword stuffing or synthetic endorsements.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://developers.google.com/search/docs/fundamentals/seo-starter-guide
- https://developers.google.com/search/docs/fundamentals/ai-optimization-guide
- https://schema.org/docs/documents.html
