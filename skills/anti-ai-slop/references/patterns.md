# Evidence and implementation review

## Code
- Verify APIs against installed types, implementation or versioned documentation. Plausible names are not evidence.
- Keep one owner for a concern. Reuse an existing boundary before adding another helper, policy layer or dependency. A single-use abstraction can still isolate a meaningful invariant.
- Check failure propagation, cleanup, cancellation and input boundaries where relevant. Do not add universal retries or blanket catch blocks.
- Tests should distinguish correct behavior from a plausible broken implementation. Avoid assertions that merely restate the code.
- Remove unused scaffolding and comments that obscure intent. Keep explanations of constraints, non-obvious choices and public contracts.

## Writing and evidence
Read `../natural-editorial-writing/SKILL.md` when prose is central. Prefer supported specifics over confidence language. Do not manufacture citations, benchmark results, customer stories or causal claims. Label synthetic examples and separate measurements from estimates. Repetition needed for technical precision is legitimate.


For a failed request, trace the state transition and error owner rather than adding another retry wrapper. Check whether cancellation is distinct from failure. For a benchmark claim, record the command, workload, environment and comparison baseline; a successful build is not evidence of a speed improvement. For an API integration, test a representative invalid response as well as the expected response. Do not rename ordinary variables or extract functions merely to make a diff appear sophisticated. Review whether another maintainer can locate the behavior and understand the contract.

## Reference entry points
Use the installed project's types and tests first. For documentation and accessible behavior, consult https://developers.google.com/style and https://www.w3.org/WAI/ARIA/apg/ as relevant; these links are reference entry points, not claims that a particular artifact conforms.

## Select checks by the changed contract
For identity or permission changes, exercise an unauthorized case. For stored data, consider existing records and old readers. For queues and caches, consider repeated requests, invalidation and cancellation. For numeric work, establish units, representative scales and an independent reference result. For ML changes, check split provenance, leakage and comparison against a simple baseline. For interfaces, verify a realistic interaction state as well as appearance. These are selection rules, not requirements to run every check for every task.

A review finding needs a location, a triggering case and a consequence. Track uncertainty explicitly. A suspicious pattern may justify one targeted inspection; it does not justify a rewrite or repeated review loops. Once the relevant checks pass, deliver the result and disclose any material verification limits.

## Use the harness evidence
For UI changes, use available `artifact_check {operation:"ui",path:...}` on the complete changed component, then inspect rendered states. Edit hooks and `quality_review` share bounded source cues; do not repeat a manual catalog-wide audit. Address concrete defects and explicit user requirements first. A style cue is advisory; a working task, truthful content and readable interface are acceptance criteria.

For code, inspect the existing owner and callers before creating a wrapper, service or duplicate component. Check swallowed failures, broad type escapes, unawaited work and invented APIs against the actual runtime. For prose and analytical work, delete stock filler and unsupported numbers while preserving evidence and qualifiers. Reuse useful boilerplate required by the framework or format.

## Website necessity review

AI website work fails one underlying way: it optimizes for visible completeness and apparent sophistication instead of contextual necessity. Every section, claim, widget and page below must earn its place for this reader; cut what does not. Each mode names its harness mechanism: source signals fire on edits and `artifact_check {operation:"ui"}`; `code_quality prose` reports stock phrases and copy rules; `design_audit` inspects rendered output; skills and observer-book passages carry the doctrine. The full 200-rule website checklist is `docs/ANTI-SLOP-CHECKLIST.md`, distilled for review in the observer-book `anti-slop` chapter.

Backstage leakage (reader never came for this):
- Implementation-detail leakage — frameworks, hosting, databases, build and deploy names in reader copy. Signal `prose-implementation-leak`; copywriting "No implementation leakage" rule; observer `web-design.no-leakage`.
- "Built with X" obsession — stack badges without relevance. Same mechanisms; keep a name only if the reader chose it (payment rails on pricing) or the page documents it.
- Needless technical jargon — APIs, pipelines, schemas, embeddings, agents, orchestration. Signal `prose-implementation-leak` (weak-term cluster); cut unless the audience needs the term.
- Internal terminology leakage — module, agent, table, pipeline and environment names. Same signal; rename to reader vocabulary or cut.
- Design-system leakage — typography, spacing, tokens, components, 8-pt grids narrated to users. Same signal; the system is felt, not announced.
- Developer comments turned into copy — README-shaped text exposed to users. Same signal (partial); read stranger-facing pages aloud and cut what sounds like documentation of the build.
- Process narration — how content was generated, reviewed, linted, deployed or maintained. Copywriting rule; cut unless the page documents the process.
- AI provenance clutter — "AI-assisted", "agent-generated", "AI-powered workflow" when irrelevant. Signal `prose-ai-provenance` plus `code_quality prose` patterns.
- Fake transparency sections — "How this site works", "Our process", "Methodology", "Architecture" pages nobody needed. Signal `prose-transparency-section` plus the `transparency-heading` prose rule.
- Premature documentation — internals nobody asked about. Reviewer check: was this question asked, or anticipated for an audience of one?
- Change-log creep — public notes for insignificant visual or technical changes. Reviewer check: would a user change behavior knowing this?

Copy without content:
- Excessive meta-copy — text describing the page instead of providing content. Reviewer check: delete the sentence; is anything lost?
- Meaningless microcopy — decorative sentences with no information. Copywriting microcopy section; every string must carry load or leave.
- Redundant explanations — restating a heading directly beneath it. Copywriting headline rule; the subhead must add, not echo.
- Overexplaining simple UI — narrating obvious buttons, navigation, search, filters. Reviewer check: if it needs explaining, simplify the control first.
- Generic benefit copy — "unlock your potential", "built for everyone", "empowering creativity". `code_quality prose` patterns plus signals `prose-buzzword-stack` and `prose-unlock-cta` (the latter fires on a single transformation CTA).
- Vague headings — single-word Discover, Explore, Insights, Solutions sections. Signal `prose-vague-heading` (navigation variants trip `prose-vague-nav`).
- Buzzword stacking — "AI-powered, intelligent, contextual, seamless, next-generation". Signal `prose-buzzword-stack`; one specific verifiable claim per adjective.
- Fake specificity — precision with no practical relevance. Signal `prose-metric-theater` plus the `metric-without-basis` prose rule for numbers; reviewer check for the rest.
- Metric theater — "10x faster", "99% smarter" without basis. Same mechanisms; add measured-where/on-what/against-what or cut.
- Badge spam — "Fast, Secure, Modern, Private, Open" with no evidence or need. Reviewer check; trust-theater variants also trip `prose-theater-claim`.
- Privacy, accessibility and security theater — advertising the posture instead of embodying it. Signal `prose-theater-claim` (skips policy pages, which legitimately explain).
- Self-congratulatory copy — elegant, thoughtful, minimal, rigorous, carefully designed. Signal `prose-self-praise` plus prose patterns.
- Artificial editorial language — "a carefully curated exploration of…" instead of saying what it is. Same mechanisms; name the thing.
- Mission-statement fabrication — grand philosophy for a small practical site. Signal `prose-mission-speak`; state what it does and for whom.
- Fake brand voice — a utilitarian site speaking startup. Reviewer check against the copywriting voice section.
- Narrative padding — a story arc where a sentence suffices. Reviewer check: compress to the sentence, keep it if nothing is lost.
- SEO padding — unnatural keyword-repeat paragraphs. Reviewer check: read aloud; if a human would never say it, cut it.
- Fake testimonials — generic quotes from invented or contextless users. Signal `placeholder-copy` for invented names; never manufacture customers, quotes or numbers.
- Meaningless labels — "Insights, Discover, Explore, Solutions, Resources". Signal `prose-vague-nav`; name each destination.
- Unnecessary FAQs — questions nobody realistically asks. Reviewer check: source each question from a real user contact or cut it.

Structure without scale:
- Section proliferation and obvious filler sections — slices added because pages are "supposed" to have them. Signal `ui-section-sprawl`; merge thin sections into plain flow.
- Cardification — everything in cards where plain text wins. Signal `ui-card-cluster`.
- Overbuilt footers — huge link structures for tiny sites. Signal `ui-footer-bloat`.
- Excessive iconography — icons beside every heading. Signal `ui-icon-density`.
- Nested navigation for tiny sites — hierarchy without content to justify it. Reviewer check: flatten until every level earns its click.
- Decorative taxonomy — categories, tags, filters over very little content. Reviewer check: search and one list beat a taxonomy under ~50 items.
- Dashboardification and data visualization without data need — KPI panels and charts as professionalism costume. Reviewer check: what decision does each number serve?
- Fake sophistication through motion — gradients, blobs, particles, parallax, glow, animated grids. Signals `ui-decoration-cluster` and `ui-continuous-motion`, plus `design_audit` slop kinds on rendered output.
- Content duplication across pages — About, Home, Mission, Philosophy saying the same thing. Reviewer check: one canonical page, links elsewhere.
- Template section order — trusted-by, features, how-it-works, testimonials, pricing, FAQ, final CTA in the stock sequence. Signal `ui-template-sequence`; order sections by this product's own argument.
- Centered everything — every block centered, no reading axis. Signal `ui-centered-everything`; center only short display lines.

Generated-UI ornaments (never produce):
- Accent rails — colored left borders or pseudo-element bars on cards. Signal `ui-accent-rail`; rendered `repeated-heavy-left-border`.
- Dot markers — small (often glowing) dots before labels and chips. Signal `ui-dot-marker`; rendered `decorative-dot-marker` (includes sibling-layout dots).
- Glow dots — pulsing/blinking status dots: ping-pattern spans, blink/pulse/glow keyframe loops, haloed dots, literal ●/•/emoji prefixes. Signal `ui-glow-dot`; rendered `text-status-dot` for the text form.
- Live pills — animated status capsules (LIVE/ONLINE/ACTIVE and variants). Signal `ui-live-pill`; rendered `animated-status-pill`.
- Eyebrow pills — pill/badge kickers above headings ("Introducing X"), pill clusters that decorate. Signal `ui-eyebrow-pill`; rendered `eyebrow-pill`.
- Fake status labels — static BETA/NEW/LIVE/AI POWERED/COMING SOON pills without a backing fact. Signal `ui-fake-status-label`; verify a version, date or observed data.
- Icon tiles — icons boxed in tinted or bordered rounded squares. Signal `ui-icon-tile`; rendered `icon-tile`.
- Pill clusters — capsules as the default container for ordinary labels. Signal `ui-pill-cluster`; rendered `slop-pills`.
- Hype badges — NEW, BETA, AI, FAST labels without meaningful state. Signal `ui-badge-spam`; rendered `hype-badge-cluster`.
- Gradient text — clipped gradient words inside headings. Signal `ui-gradient-text`; rendered `slop-gradient-text`.
- Glass panels — blur plus translucency as the default container. Signal `ui-glass-panel`; rendered `slop-glass`.
- Oversized type — 80px+ display headlines on ordinary pages. Signal `ui-oversized-type`; rendered `slop-oversized-type`.
- Rounded excess — giant radii on every container (the bento tell). Signal `ui-rounded-excess`; rendered `slop-heavy-radius`.
- Emoji chrome — emoji in headings, buttons, links or tabs. Signal `ui-emoji-ui`; rendered `emoji-in-chrome`.
- Fake terminals — prompt lines and status codes outside code samples. Signal `ui-fake-terminal`; rendered `fake-terminal-decoration`.

Interface correctness (objective defects, not taste):
- Missing alt — visible images without any alt attribute. Signal `ui-missing-alt`; rendered `missing-image-alt`.
- Unnamed controls — buttons, links, inputs and widgets with no accessible name. Signal `ui-icon-button-name` (icon-only buttons); rendered `unnamed-control`.
- Skipped headings — h1 straight to h3 and similar outline jumps. Signal `ui-heading-skip`.
- Autoplay and custom cursors — marquees, autoplaying carousels, hidden native cursor. Signals `ui-auto-carousel`, `ui-custom-cursor`.

Invented labels and leaked work-thoughts (never produce):
- Made-up SaaS labels — adjective-stacked headings ("AI-Powered Analytics", "Smart Dashboard") and vague nav (Platform, Solutions, Insights, Discover). `code_quality prose` rule `slop-label` plus signal `prose-vague-nav`; name the concrete capability or destination.
- Work-thought leaks — first-person build narration and making-of commentary in reader copy ("I designed", "this section showcases", "as an AI"). Signal `prose-work-thought-leak` plus `code_quality prose` patterns; cut the narration, keep the fact.
- Bare vanity metrics — k/M+ user counts, 4.9/5 ratings, #1 claims without a nearby basis. Signal `prose-metric-theater` plus the `metric-without-basis` prose rule; add measured-where/on-what/against-what or cut.

Color without a system:
- The stock AI palette — indigo or violet into purple or pink gradients on a product that does not own them. Signal `ui-stock-palette`; derive a value ramp and one accent from the brand.
- Hue sprawl — a dozen ad hoc hex values, no tokens. Signal `ui-hue-sprawl`; a small token set reused everywhere.

Generated-sounding prose:
- Delve/tapestry/testament phrasing, em-dash floods, rhythmic triads, repeated sentence openers. Signal `prose-ai-tells` (the shared `code_quality prose` checker, run automatically on authored copy).

Features without need:
- Premature ecosystem language — a few pages called a "platform", "ecosystem" or "suite". Reviewer check: name what it is today.
- Feature inflation — one feature renamed into four "capabilities". Reviewer check: would the changelog list four items?
- Capability duplication — multiple elements doing the same thing. Reviewer check with the rendered page: one path per task.
- Unnecessary onboarding — tours and modals for self-explanatory sites. Signal `ui-onboarding-nudge`.
- Overdesigned empty states — motivational paragraphs where "No results" suffices. Reviewer check: state, cause, one action, stop.
- Premature personalization — themes, accounts, saved states before need. Reviewer check: ship the need first, persist later.
- AI feature insertion — chatbots, summarizers, "Ask AI" because AI is available. Signal `ui-ai-widget`; a paragraph that answers beats a widget that chats.

Scope discipline (the agent, not the artifact):
- Agentic overreach — inventing pages, features, copy, categories or architecture beyond the request. Observer `web-design.necessity`; build only what was asked, propose the rest.
- Completion theater — visible complexity mistaken for finishedness. Same passage; the master test: remove the element — does the reader lose anything? If not, it was theater.
