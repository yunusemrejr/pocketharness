---
name: copywriting
description: 'Write product and marketing copy: positioning, headlines, calls to action, landing pages, pricing, onboarding, errors and microcopy. Use for clear, credible user-facing language.'
---


# Copywriting (for product UI & marketing)

The single most under-weighted lever in web design: a beautiful layout with weak copy converts *worse* than a plain layout with honest specific copy. The reader skims (they never read), so every sentence must do one job: **make the next step obvious and worth taking**. This skill is the words half of `ui-ux-principles`/`web-patterns`; pair it with `storytelling` (the arc) and the case index at the end.

## The hierarchy of a page's words (and what each must do)

1. **Product name** (only one job: be ownable; never explain).
2. **Value proposition** (the one line that wins the skeptical 5 seconds: *what you do* for *whom*, in their words, with a specific number or outcome where you have one — "Cut your close from 9 days to 3" beats "Accelerate finance operations" by 10:1; the classic shape: "[Outcome] for [who] without [the thing they hate]").
3. **Headlines** (per section, one thought each; the headlines-read-only path must form a coherent story — write the headlines, read them as a standalone paragraph: that paragraph is your page at scroll-speed, if it's incoherent the page is incoherent).
4. **Body** (2-4 sentences per section max; one idea, a concrete detail, done; the "and for example" is the second sentence's job).
5. **Proof** (numbers with a base — "38% faster, measured over 400 teams in Q3" — not "faster for thousands of teams"; logos (named, not silhouette-grayed-out-if-you-can't-name-them — a greyed logo strip is a *claim* of logos you didn't earn); one quote per proof block (a person, a title, a number in the quote where possible)).
6. **CTA** (one primary per screen — see below).
The hierarchy rule: each level is *shorter and more specific* than the last; a value-prop that needs a second sentence is not a value-prop.

## No implementation leakage (reader-serving details only)

A core rule for every user-facing page: never expose internal architecture, AI usage, agent workflows, repositories, frameworks, hosting choices, pipelines, generation processes, or other developer-facing details — unless the page explicitly exists to document those subjects. Every visible detail must serve the reader's purpose in that context.

- The test is one question: *does this line help the reader decide or do the next thing?* If it only proves how the thing was made, cut it — the reader came for the outcome, not the backstage tour.
- The usual leaks: "Built with X" / "Powered by Y" badges (unless Y is the reader's concern — payment rails on a pricing page earn their line, the CSS framework never does), "AI-generated/powered" labels on products the reader didn't buy for AI, repo and framework names, pipeline/deploy mentions, "our agents/workflows" meta-talk, version numbers outside changelogs.
- The docs exception: when the page *is* the docs (guides, API reference, changelog, status, engineering blog), the reader's purpose *is* the implementation — then specificity is the virtue (versions, endpoints, configs, migration steps). The rule inverts; it doesn't vanish. A marketing page linking *to* those docs is how the curious reader gets depth without every page paying the tax.
- This rule is one entry in the harness-wide website necessity catalog (`anti-ai-slop/references/patterns.md`): automated source signals (`prose-implementation-leak` and kin), `code_quality prose` rules, `design_audit` rendered checks, and observer `web-design` passages enforce the rest.

## Headlines: specific > clever (the real rule)

- **Specific beats clever, every time** ("Reports that write themselves" is a claim; "Your Friday report, drafted from your tools before you leave Thursday" is a *scene* — the scene is what a skimmer actually reads).
- The 5-word test: can you say it as "I made X so Y can Z without W"? If yes, the headline is a *variance* of that (clarity was never the risk).
- **Number claims need a base** (who, what, how long, n) — "10x faster" is either a trust *tax* (no base) or a claim you can't substantiate; the FTC/your-own-sleep test: could you defend it in a line?
- Superlatives without a basis ("the best", "the largest team of experts") = the #1 generated-copy tell (and the #1 anti-slop-killword — see `anti-ai-slop`).
- Front-load: the skimmer reads words 1-4; the verb or the number goes first ("Cut reporting time 70%" — not "Introducing a way to cut reporting time").
- **No headline + subhead that says the same thing twice** (the "Headline: Ship faster. / Subhead: Ship your code faster, faster." loop — the subhead adds the *how or the who*, never a repeat-in-synonyms).

## CTA language (the button that does the work)

- **Verb + outcome, in the user's first person of the outcome**: "Start your trial", "Get my report", "See pricing", "Book a demo" — **never "Submit", "OK", "Click here", "Learn more" as the *primary*** (learn-more is a secondary at best; "click here" is banned outright: it's a pointer with no destination).
- **One primary CTA per screen** (the largest, most saturated, visually-the-weightiest element on that screen — if two elements tie for "most important", the page has two products); secondary CTAs are text-links or ghost buttons, never equal-weight.
- **First-person CTAs convert measurably higher in most consumer tests** ("Get started with my data" > "Get started") — use it where the voice allows (consumer/B2C), not in formal B2B (it can read as cutesy there).
- **After the fold**: the CTA repeats (mobile: a sticky bottom bar with the CTA + one line of value) — the skimmer who reached the bottom without clicking needs the offer again, *with the same words* (CTA copy consistency across the page = the action feels like one thing, not many).
- Destructive/reversible: "Delete" is specific (not "Remove"), confirms name the object + the irreversibility (the microcopy section covers it); **an undo beats the confirm modal** for anything reversible (a "Deleted — Undo 5s" toast is both faster and kinder than a modal).

## The landing arc (words as narrative) — the 10-second read

The landing page is a story told at scroll-speed (full arc in `storytelling`); the *words* per beat: **Context** ("Close week eats your Fridays" — the reader's world, their language, not the vendor's jargon) → **Problem** (2-3 bullets, the specific pains, named with *their* words — mine them from 3 customer calls/your ticket titles; "export, pivot, screenshot, paste" is *the* problem sentence in every finance tool because it's what users *say*) → **Turn** (one line reframing: "What if it wrote itself?") → **Solution** (the product in one line + the 3-step how) → **Proof** (number + logo + quote, one each) → **CTA**. The discipline: *each section's header is one clause of the story, the subheads are the beats, the CTA is the ending* — and the whole thing works with only-the-headers-read (the 10-second test is real: time a skimmer, headers only).

## Microcopy (where the product *sounds*) — the working reference

The rules per state (copy is *designed* here; it goes in PRs with screenshots):

- **Errors** (the order is the law): what happened → why (if knowable, in one clause) → what to do (the fix as a link/button). Blame the system or nothing, never the user: "We couldn't save your changes" (acceptable) / "Invalid input" (telling them *they* failed, about nothing) — and the specific fix: "We couldn't connect to your bank. Check the account number or try again in a minute." 429 = "You're going fast — slow down for a minute, don't worry, nothing was lost." 401/403 = "You're signed out" / "You don't have access to that — ask an admin" (the *next action* is the line, not the code). A bare "Something went wrong" with a request-id is the floor, not the standard (tie it to `api-design`: the id exists so support can find it *and* the user can *paste* it — say so: "If this repeats, send #REQ-8f3a to support").
- **Empty states** (the most-loved, most-skipped copy): what the space is + why it's empty (or that it's ready) + the next action as a button: "No invoices yet — create your first and we'll file it in the right period." Not "No results" (which is also *wrong* — "no results" is for search; an empty *list* is a start-story, an empty *search* is a retry-story: "No matches for \"xyz\" — try a shorter term or clear the filters" *names the applied filters as removable links* — the retry path is the copy).
- **Loading**: named, honest, with a time-sense where possible: "Syncing 3 of 10…" (the progress is the copy) / "Loading your report…" (not "Loading…" about *what*) / "This usually takes ~20 seconds" (a wait you *named* is a wait you survive — an unbounded spinner at 30s becomes "is it dead?"); > 1s: put in a skeleton, not a spinner (the pattern, `web-patterns`).
- **Success/toasts**: past tense + the object + (undo if available): "Copied." (the minimal, good), "Invoice #42 sent to A. Chen." (the useful), "Team settings saved." (the quiet confirm) — a toast is the *shortest confirmation that proves the thing happened*; no exclamation marks by default (a "!" on every save is a bark after 50 saves — reserve it for the first-time/special moments; the *first* success of a new feature *can* celebrate, the hundredth can't).
- **Onboarding**: each step = what's happening (a label) + why (one clause) + progress ("2 of 3") + a skip/out path (a story with no exit is an abandonment event); the *first value* moment gets the one-sentence "you just did the thing" (the aha is *named*: "That's your team's first shared report — anyone you add can edit it right now.").
- **Forms**: labels always (a placeholder is not a label — it vanishes; a floating label is acceptable *only* with a lib that nails the a11y); helper text is for *rules* ("3-50 characters, no spaces"), never for "what this is" (the label said what); validation on blur (never on first keystroke), the message *next to the field*, the fix in it ("Use 10+ characters" is the fix, "Too short" is not); submit button states: "Save" → "Saving…" (spinner *in* the button) → success goes to the *screen* (a "Saved" state or the return to the flow), errors inline (never a toast for a field failure — the toast closes, the field's error must stay).
- **Passwords**: the meter shows *what the meter measures* (a real check — length + breach-list, not the "1234/ABC/symbol" game that teaches people to type "P@ssw0rd!"); show/hide toggle; the autofill fields *named* (username/email/password so the managers work — the a11y-and-practical rule).
- **Destructive confirms**: the action + the object + the irreversibility + (the undo if there is one): "Delete 14 exports? This can't be undone." (the count is the line — it makes them *pause at the number*); type-to-confirm is for the org/workspace nukes only (naming "ACME, Inc." to delete the org — the typing *is* the second confirmation); the danger action lives at the bottom, red, never next to the primary (the layout does the warning the copy should do — `web-patterns`).
- **Numbers in UI copy**: the format is copy (Intl.NumberFormat — 1,204 / 1.204 / 1 204 per locale; the % placement per locale, the currency via Intl + the *symbol or the code* by audience — "USD 1,204" for a global B2B, "$1,204" for a US one; never a hardcoded thousands-separator in a string: "1" + "204" in a localized template = the German bug).
- **The changelog**: a version = 3 stories, not a pile (the 1 headline change + 2 improvements + the "also" line); each item gets the *why-line* in the user's terms ("Exports now include the filter state — you stop re-applying filters after a download"); a release with only "various bug fixes" is an *unreleased* release (write the one thing that fixed for someone: "Fixed: the close report double-counted adjusted entries" beats "various").

## Pricing & plans copy (the money words)

- **Plan names = a tier axis** (pick *one*: Solo/Team/Scale, or Free/Pro/Business — the axis is *who* or *how many* or *how much*; mixing axes (Starter/Pro/Enterprise-ish "Scale") = the "which is for me?" scroll); the highlighted plan = your honest answer to "which one" (name it "Most popular" *only if the data says so* — it's a claim).
- **Feature lists in the user's words** ("Unlimited seats" not "Max users: ∞"; "Priority support (2h first response)" — the *concrete* is the feature, the adjective is the garnish); the comparison matrix (the power-user path) groups rows (Features/Limits/Support), checks and "—" (never "N/A" for "not included" — it's not not-applicable, it's *not there*), and highlights the recommended column (the background, not the border-thumping).
- **The meter/upgrade moment**: the usage line states the number + the reset + the cost-of-more in one line ("74% of your 5,000 seats used — resets Monday · Next seat: $12/mo") — the *honest near-limit warning at 90%, not the wall at 100%* (the wall is the refund-request generator; the 90% nudge is the revenue with the goodwill); a paywall explains *what unlocks*, not what's missing ("Unlock unlimited exports, SSO, and audit logs") — the *gain* frame, never the *loss* frame.
- **The annual toggle**: default to the *annual if you want annual* (it's a choice-of-UI, be deliberate about it — "Save 20%" is the standard anchor, name the month-equivalent: "$80/mo billed yearly ($960)" — the small-print honesty is the trust line).

## Voice & tone (the one-page decision, not the 40-page book)

- **Pick a register and hold it** (formal ↔ playful is one axis, one position; the *error page* is written by the same voice as the hero — a cheeky 404 under a sober checkout is a different company); the test: read the 10 loneliest strings (the errors, the legal-touching lines, the empty states) aloud — a real human of that company says them.
- **Second person** (you/your) > first-person plural (we — sparingly, for the brand voice) > third person (the product: "the report" not "it" — *name the thing*, the user knows what they're looking at); **active by default** (the system does: "We deleted the file" ✓ / "The file has been deleted" ✗ — the passive is where "something" went wrong and no one's accountable); **contractions** in a conversational register (don't ✓, do not ✗ — "do not" is a warning voice, use it *only* in warnings); no exclamation marks by default (above); no jargon in the reader's path (the "ingest your data into our pipeline" → "import your data").
- **The legal-adjacent** (privacy, terms, consents): human-first — the one-line summary up top ("What we collect, why, and how long — here's the short version" → the detail), the TOC that jumps, the "your rights" section *named*; the consent banner (where required): plain summary + "Only necessary / Accept all" (the GDPR floor — never a pre-checked box, never "by continuing you accept" as the *only* mechanism in the EU/UK, the legal in `web-security`); the ToS is not yours to soften, but it's yours to make *navigable*.

## Localization-safe writing (writing for extraction from day 1)

- **No idioms in the critical path** (the conversion, auth, errors): "hit the button" dies in most languages; the literal ("click Save") is the safe one; the idioms are allowed in *marketing* copy (translatable with effort, not *blocked*).
- **CLDR plurals, not English plurals**: "You have {n} new messages" breaks in Russian/Polish/etc (3+ plural forms) — the template: the "you have" + the number + the noun in *the grammar slot* (the i18n lib's plural API does it; the rule for *writers*: never hardcode the "s", never split a sentence around the count — "Reports: {n}" is extraction-safe, "You have one new report" is not).
- **String-length budget**: German +30%, Turkish/Russian +40% — the button says "Save" (4) and "Gespeichert" (10) and "Enregistrer" (11) — *design* the button for +50% (the `design-systems` token: `--chip-min-w` from the longest translated string); the ellipsis is automatic — write the *truncatable at the end* sentence (the number/verb last where the ellipsis-safe).
- **Gender/plural/case**: the templates take variables in the grammatical slots (the lib handles it); the writer's job is to *keep the variables free* (no "the report's owner" fused with the name — the "the {name}'s report" is the German case-inflection bug).
- **Never hardcode** (the non-negotiable list): dates/times/currency/numbers (Intl), unit order (Jan 5 vs 05/01), the "OK/Cancel" order (it's per-locale: OK|Cancel in the US, **Annuler | OK** in FR — the *right* order is the native one; the "we standardize OK on the left" is the localization finding), addresses (a "state" field is "province" is "prefecture").

## The anti-slop word list (the generated-copy kill list)

The AI-default words — if a draft has 3+, it was not written, it was *generated*: "seamless/robust/streamline/leverage/harness/delve/empower/unleash/next-generation/state-of-the-art/game-changer/reimagine/revolutionize/effortless/frictionless/enterprise-grade/scalable/turnkey/white-glove" + the sentence shapes: "Introducing X — the all-in-one platform to Y", "In today's fast-paced world…", "We're passionate about…", "Our commitment to excellence…". The fix per word: *say the specific thing it's hiding* (seamless = "no setup between steps" or cut the word; robust = the failure it survives; enterprise-grade = the named feature (SSO, audit logs, 99.99%)). The test: a stranger reads the line and can name the product's *actual* thing — if they can only echo the adjectives, the words failed. (Pairs with `anti-ai-slop` for the visual/code tells.)

## Cases (the copy to study, per beat)

- **Stripe** (docs-as-narrative): the "Get started" is a *task story* (the headline names the step, the code block is the proof, the next beat is one link away) — the pattern: **each docs page = one task, one promise, one done-state**.
- **Linear** (product page): "Project management for software teams" (the value-prop is 6 words, *for whom is in the sentence*) → the problem beat (the "disconnected, bloated" state, in *the reader's* words) → the answer → the demo-as-scroll — the sparseness is the copy: **they cut until only the scene remained**.
- **Vercel** (developer trust): "Frontend Cloud" (a name that's a category, not a feature) — the copy does the *positioning* in two words and lets the docs/demo do the rest; the changelog-as-product (their shipping log is *marketing copy* written in the user's verbs: "Deploy from GitHub in one click").
- **Notion** (the template-as-copy): the empty-state templates *are* the onboarding copy (a "Project Wiki" template with 3 pre-filled example lines = the fastest "here's what this is" in SaaS — **the sample content is copy**, write it like it's the product's voice, because it is).
- **Figma** (the explainer): "What is Figma?" is a story *for the skeptic* (what's a design file? why does the browser run it?) — the audience-matched narrative: the page is written for the one person who's about to leave, not the one who's already sold.

## The 2-minute audit (any page, in 120 seconds)

1. Close the images. Read headers only. Coherent story? (no → the arc is broken, fix the headers, the rest follows).
2. The CTA on the first screen: verb + outcome? one primary? (the tie = the product).
3. The 3 longest sentences in the page: over 25 words? (they're not read — cut or split; the web reader's sentence = 15 words).
4. Any error/empty state without the *fix line*? (the "what to do" is missing — the #1 find).
5. The 5-word test on the value-prop (X so Y can Z without W — can you say it?).
6. The kill-word pass (the list above, grep the draft).
7. One number in the whole page? (a page with zero specific numbers is *all* claim — find the one defensible number, put it in the proof beat).
8. Read the 404 + the 429 + the login-error aloud. Same person wrote them all? (the voice drift check).
9. The leak pass: any framework/repo/hosting/AI/pipeline mention the reader didn't come for? (cut it unless the page is the docs — see above).

## Detailed coverage

Marketing language for product/web UI — the value-proposition line, headline writing (specific > clever), CTA language (verb + outcome, never "click here"), the landing-page narrative arc, pricing copy, microcopy (errors, empty states, toasts, loading, onboarding, forms), tone/voice discipline, localization-safe writing (CLDR plurals, no idioms in critical paths), and the anti-AI-slop word list. Use when writing or reviewing any product copy — landing pages, UI strings, emails, changelogs — or when pages "feel flat" and the problem is the words, not the pixels.
