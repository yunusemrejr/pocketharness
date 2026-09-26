# Web UI Stack Selection: patterns and examples

## Match the delivery environment
A static cPanel page may need HTML/CSS plus a small enhancement; a data-heavy application may justify a reactive framework and mature grid. SSR, hydration, CSP and offline requirements change the choice. Inspect the existing lockfile and build process before selecting React/Vue/Svelte-specific packages. Web components can cross framework boundaries, but shadow DOM styling, forms, events and accessibility require integration tests.

Use Alpine for modest stateful enhancements when it fits the project. Keep jQuery where it is an intentional dependency; do not add it just to replace querySelector, and do not rewrite stable legacy behavior without a reason. A headless library supplies interaction mechanics, not the product's labels, hierarchy or visual design. Styled systems trade speed for theming constraints. Copied source components become code you maintain.

## Dependency/CDN contract
Pin production versions; never use `latest` in a deployment artifact. Prefer a project build when it provides repeatable bundling and supply-chain checks. A CDN may suit a prototype or static host, but evaluate outage behavior, content integrity, privacy, CSP, module imports and offline use. SRI applies to supported script/link loading; it does not magically secure an entire ESM dependency graph. Self-hosting removes a runtime dependency only if updates and provenance are maintained.

Example decision record: existing vanilla page; one disclosure and validation state; no router or SSR; choose native details plus a small local module. For a searchable multi-select, compare an accessible primitive against custom implementation effort using keyboard, IME and screen-reader cases.

## Adoption experiment
Implement one difficult real component rather than a toy button. Measure bundle delta, load/interaction behavior, theming effort and test burden. Check SSR-safe imports and tree-shaking in the actual bundler. Verify focus behavior under portals, touch and zoom. Do not assert that a library is maintained, deprecated or universally best based on an old catalog; consult its current primary release/docs and project constraints.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity
- https://alpinejs.dev/start-here
- https://jquery.com/
- https://www.w3.org/WAI/ARIA/apg/
