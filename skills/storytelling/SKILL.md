---
name: storytelling
description: >-
  Narrative for product and web — the page as a spatial story (the 3-resolutions rule: it must work with headers only, with sections, with details), the landing-page arc (context → problem → turn → solution → proof → CTA), the homepage section-by-section script, scrollytelling (the device, when to use it, when it's the wrong tool), onboarding as a 3-act narrative with the first-value moment, case-study structure (the before/after with the number and the quote), changelog/release narrative, brand narrative in 1/5/50 lines, the anti-slop narrative tells ("In today's fast-paced world", hero slogans that say nothing, case studies without numbers), and a case index (Stripe/Linear/Vercel/Notion/Figma). Use when writing or restructuring any marketing page, docs narrative, onboarding, case study, or changelog — or when a page "has everything and still doesn't convert" (the arc is broken, not the design).
---

# Storytelling (product & web narrative)

A web page is a story told *spatially*: the page is the chapter, the scroll is time, the section is the scene, the headline is the beat. The reader is skimming and half-reading, so the story must exist at **three resolutions simultaneously**: the **headers-only** path (a skimmer reads only the H1s — those H1s must form a coherent sentence-level story), the **section** path (headers + first line each), and the **detail** path (the full read). Most "doesn't convert" pages have a fine detail path and a broken headers path — the skimmer (your only fast reader) got a pile of fragments. **Write the headers first as a standalone paragraph; if that paragraph isn't the story, no amount of design saves the page.**

## The landing arc (the workhorse — 6 beats, one job each)

1. **Context** — the reader's world, in their language ("Close week eats your Fridays"). Two lines max. You must *be* them, not pitch them.
2. **Problem** — the specific pain, named (3 bullets, ideally words lifted from 3 customer conversations or ticket titles — "export, pivot, screenshot, paste" is the problem sentence of every finance tool because it's what users *say*).
3. **The turn** — one line that reframes the problem as solvable ("What if the report wrote itself?"). This is the whole page's hinge; if it's weak, the rest is a feature list.
4. **Solution** — the product in one line + the 3-step how (numbered, verb-first: "Connect" "Configure" "Done" — the how must be *3*, not 5; if it's 5 in reality, group to 3 or the page is lying).
5. **Proof** — the number + the logo + the quote, one each (a defensible number with a base — "38% less time, 400 teams, Q3" — beats "10x faster"; logos only if you can name them to a lawyer; the quote carries the *voice*, the number carries the *truth*).
6. **CTA** — one action (`copywriting` for the words). The arc *ends* with the action; a page that ends with "learn more about us" failed at beat 3.

The discipline: *each beat is one section, one header (one clause), one subhead (the beat), one action of the reader's attention*. The 10-second test: scroll at 10s with headers visible only — headers only must read as: problem → hinge → answer → proof → go. That's the whole page at skim speed.

## The homepage script (section-by-section, the typical 7-9)

Hero (the value line + one CTA + the product visual — **a screenshot of the real product beats an illustration every time for a software product**: the screenshot is the proof that it's real; the "hero video" is an optional *second* — no audio, a poster frame, < 15s loop, `media-in-web` for the mechanics) → logo strip (social proof; the names you can name) → problem (the beat 2 content, can be its own section) → how it works (the 3 steps, each with a number + a visual + a clause) → features (**grouped by job, not capability**: "See" / "Act" / "Share" — each group = one H2 + 3 items max; the capability-list ("SSO, 2FA, audit, roles, SCIM…") goes to the enterprise band, not the features band) → proof deep (one case study: the number + the quote + the screenshot — the full format in `case study` below) → (pricing, if public — `copywriting`) → FAQ (the objections, in voice — 5-10, the *real* ones from sales calls, the "does it work with X" is the converting answer) → footer (the sitemap + the legal, quiet — the footer is not a story).

- **The "enterprise band"**: the trust row for the skeptic at the bottom (SOC 2, SLA, SSO, the security page link — one calm strip, not a wall; the "we're enterprise-ready" page is separate, linked from this strip).
- Section count is not the point — **every section must *advance* the arc** (delete-test a section: remove it, does the story still land? if yes, it's decoration — the decoration budget is 1 section (the story/culture band), not 3).

## Scrollytelling (scroll as time — the device, and when not to)

The device: the scroll *reveals the story in sequence* — the classic construction is **sticky text left / changing visual right** (the text holds the beat, the visual advances with it: the NYT long-form pattern; in product: "how it works" where each scroll step swaps the product screenshot to the matching state). Implementation: IntersectionObserver on the beats (not scroll-jacking the page — the `motion`/`frontend-js` mechanics); one thought per beat; one small animation per beat (the advance is the animation, not a parallax storm); a progress cue (dots, a thin bar) when the story is > 5 beats.

- **Do NOT scrollytell**: docs (jump-to-section IS the story — the reader is *searching*, not reading; a scroll-locked docs page is an act of violence), app UI (function > narrative; the story lives in onboarding, not the workspace), and anything with a search box in the hero (a searching reader has no story to receive). Scrollytelling is for *marketing and explainers* — the two places the reader is in reading mode.
- **Pacing rule**: the scroll distance between beats is the *time* between them (a screenful per beat is the default; a 2-screen gap = a deliberate pause, use it once for the biggest beat — the "before the turn, the page holds its breath" move).

## Onboarding as narrative (3 acts, one job each)

- **Act 1 — Welcome (the hook)**: who we are + what they'll get, one line each, *before* any form. The "what you'll be able to do by the end" is the hook (the goal, not the steps).
- **Act 2 — Setup (the task, not the form)**: the minimum path to first value (≤ 3 steps; one input per step where possible; every field has a *why clause* ("so we can…" — the trust line, `copywriting` microcopy)); the progress ("2 of 3, ~1 min") + the **skip/out path** (a story with no exit is an abandonment event; the "skip for now" must still land them somewhere useful — a sample workspace, not a dead end).
- **Act 3 — First value (the aha, named)**: the moment must be *created* (the sample data / the one connect / the one auto-generated thing — the "your first report, made from real data in 30s") and then *named* ("That's your team's first shared report — anyone you add can edit it right now."). The aha is the story's ending: **if you can't name a 10-second moment where the product *is*, the onboarding has no Act 3 and the churn is the story's true ending.**
- The empty states after onboarding *continue* the story (the "No invoices yet — create your first and we'll file it in the right period" = the next beat promised — `copywriting`).

## Case study (the proof chapter — the structure that converts)

**Customer** (who, the kind of company — "a 200-person fintech in Berlin") → **Before** (the situation in *their* words + the baseline number — "close took 9 days, 2 analysts, 3 spreadsheets") → **Change** (what they adopted, named, in 2 sentences + the screenshot — the *product in their context*, not a stock dash) → **After** (the number moved — "9 days → 3" — the metric, the timeframe, the quote with the person + title; **the number in the quote where possible** — "we stopped dreading Friday" + "9 to 3 days" = the emotional *and* the truth) → the pull quote as the *shareable artifact* (the one-sentence version that's the pull-quote on the marketing page — a case study without a pull-quote is a report, not a story).

- The discipline: *one* number carries the case (the others are garnish), the quote carries the voice, the screenshot carries the proof-of-existence, and the **before number must be as specific as the after** (the "3x faster" with no baseline is the anti-slop tell — `copywriting`).

## Changelog / release narrative (shipping as a serial)

A release = a *chapter* in the product's serial: **3 stories, not a pile** — the one headline bet ("Exports now include your filters" — the *why-line* in user terms: "you stop re-applying filters after a download") + 2 improvements + the quiet "also" line; the version number is the episode, the date is the timestamp. The release note is a *mini-landing* (the hero = the biggest thing, then the list); **a changelog that's all "bug fixes" is a serial with no plot** (write the one thing that fixed-for-someone: "Fixed: the close report double-counted adjusted entries" names the reader — "various" names nobody). The discipline that compounds: a public changelog is the *trust asset* (the "they ship" proof, linked from the pricing page's "is this real" objection) — the "no updates in 8 months" is the dead-company tell that does more damage than the flaw it's covering.

## Brand narrative (in 1, 5, and 50 lines — the 3-zoom system)

- **The 1-line** (positioning): what it is *and* for whom, in one breath ("Project management for software teams" — Linear's; note: *for whom is in the sentence*). This line is the top of the homepage, the one-line in the pitch, the answer to "what do you do" — **it must be the same sentence everywhere** (the drift per surface is the #1 brand-weakness tell).
- **The 5-line** (the story): founded-because __(the wound that made the product) → instead-of__ (the status quo named) → we __(the one-line's mechanism) → so-that__ (the reader's outcome) → unlike __ (the differentiation, one clause). The about-page skeleton, the pitch's narrative slide, the support team's "what we actually do" answer.
- **The 50-line** (the full story): the founder's voice (a real person, a named decision, the honest struggle — the "the first demo crashed in front of 9 investors" line is the trust line; the "passionate team" is its absence) + the proof (the numbers, the milestones — the dated ones) + the same CTA as the 1-line. **Consistency check (the 3-zoom test)**: the tagline (page) = the 1-line; the about = the 5-line expanded 5:1; the press kit = both + the logo + the *same* product adjective. If the three zoom levels describe different products, the brand is a slogan, not a story.

## The anti-slop narrative tells (the generated-story kill list — pairs with `copywriting`/`anti-ai-slop`)

- The **hero slogan that says nothing** ("Innovation meets efficiency" — the 3-second test: what *is* this? a slogan that answers nothing is the #1 tell; the value line names the thing).
- The **5-paragraph "We're a passionate team"** (the culture band with no plot — the team story must have a *decision* in it (the wound, the bet) or it's filler).
- The **case study without numbers** (the words of victory, no baseline — the proof that proves nothing).
- The **feature list that reads as a spec** (no benefit sentence per item — "AES-256 encryption" is the mechanism; the sentence is "your data isn't readable without your key").
- The **"Welcome to X" opener** (the reader is already there — the welcome is the *product's*, the page's first line is the *reader's* problem).
- The **fake proof** ("Join 10,000+ users" with no date/base; a testimonial with no name/no title; a "most popular" flag on the plan with the least users — `copywriting`'s honesty rules).
- The **stale serial** (a timeline that ends 18 months ago, a changelog's last entry last year — the story stopped = the company looks stopped; the *honest* "we're small, here's what's next" beats the frozen timeline).
- The **arc-that-skips-the-turn** (problem → features, no hinge — the "so what" gap; the page informs and never *turns* — the solution section arrives as a list, not as an answer to the problem the page just created).

## Cases (the narratives to study — from the links)

- **Stripe** (docs-as-narrative): the "Get started" is a *task story* (the header names the step, the code is the proof, the next beat is one link away — each page = one task, one promise, one done-state); the pricing page is a *scale story* (the tiers as a journey — "start free, grow into…", the tiers are chapters, not rows).
- **Linear** (the problem-first product page): the value-prop is 6 words with the *audience in the sentence* ("Project management for software teams") → the problem beat (the "disconnected, bloated" state, in the reader's words) → the answer → the scroll *is the demo* (the product moving at the beats — scrollytelling as the page, done with restraint).
- **Vercel** (developer trust): "Frontend Cloud" (a name that's a *category*, not a feature — the 2-word positioning doing the 1-line's work); the docs as *parallel stories* (the CLI path and the dashboard path side by side = "your way" — the narrative accommodation of two reader types); the shipping log as *marketing* (the releases in the reader's verbs: "Deploy from GitHub in one click").
- **Notion** (templates as onboarding copy): the empty-state templates *are* the narrative — a "Project Wiki" with 3 pre-filled example lines is the fastest "here's what this is" in SaaS (**the sample content is writing**: the example lines are the voice, the structure is the story — write them like copy, because they are copy the reader will read first).
- **Figma** (the skeptic's story): "What is Figma?" is written *for the person about to leave* (what's a design file? why does the browser run it? what's the multiplayer?) — the audience-matched narrative: the page's reader is defined before the first line is written (the "who is the reader *right now*" question is the first story question, always).

## The 3-minute narrative audit

1. Headers-only: coherent paragraph with a turn? (no → the arc is broken; fix the H1s first, the body follows).
2. The 1-line: can you say "what is X for who" in one breath? Is it the same sentence on every surface? (the 3-zoom test).
3. The aha: name the 10-second first-value moment. (can't → the onboarding has no Act 3).
4. The proof: one number with a baseline? one named person? (no → the story is all claim).
5. The delete-test: every section advances the arc? (a section that doesn't is decoration; the budget is 1).
6. The reader-now: who is the reader *at this section* (skimmer? skeptic? buyer?) and does the section write to them? (the audience-mismatch is the "I read it and it said nothing to me" bug).
