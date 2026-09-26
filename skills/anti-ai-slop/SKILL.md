---
name: anti-ai-slop
description: Review code, prose, UI and analytical artifacts for empty polish, unsupported claims, unnecessary structure and missing behavior. Use for anti-slop requests or a focused final quality review; preserve intentional style and user scope.
---

# Substance Before Polish

Improve the artifact, not its perceived authorship. No heuristic can prove that work was made by AI. Use one focused pass, fix supported problems, and stop when the requested behavior is verified.

## Working method
1. Identify the audience, task and acceptance criteria. Read the nearest relevant implementation or design guidance.
2. Inspect the artifact itself. Separate observed defects, uncertain concerns and stylistic preferences.
3. Choose the smallest repair that improves usefulness. Preserve established conventions, deliberate creative choices and required wording.
4. Verify the changed behavior. Report remaining unknowns without claiming tests or visual inspection that did not happen.

## Focused checks
Read `references/patterns.md` for code and evidence review. For prose, use `../natural-editorial-writing/SKILL.md`. Check real failure behavior and claim provenance before polishing presentation.

## Interfaces
Read `../ui-antipattern-review/SKILL.md` when UI is central. For website work, apply the 200-rule checklist in `docs/ANTI-SLOP-CHECKLIST.md` (core principle, decision test, final rule) and the observer-book `anti-slop` passages. Inspect real content and interaction states. A screenshot demonstrates appearance, not keyboard behavior. When visual tooling is unavailable, use DOM, geometry and contrast evidence and explicitly limit the conclusion.

## Never produce (generated-UI ornaments)

These read as machine-made at a glance; do not write them, and remove them when found unless the user's brand or an explicit state need requires one:
- a colored left rail on cards, callouts or list items (thick or 2–3px `border-left`, or a pseudo-element bar);
- small colored dots before labels, chips, nav items or headings, glowing or not (a dot is allowed only for a real, changing state with a text equivalent);
- glowing, pulsing or blinking status dots in any form: `animate-ping`/`animate-pulse` spans, blink/pulse/glow keyframe loops, `box-shadow` halos on dots, or literal `●`/`•`/emoji prefixes before status words;
- animated LIVE-style pills and static invented status labels (`BETA`, `NEW`, `LIVE`, `AI ACTIVE`, `SYSTEM ONLINE`, `COMING SOON`) without a backing fact — a version, a date, observed live data;
- eyebrow pills or badge kickers above headings ("✨ Introducing X"), and pill clusters that decorate instead of selecting, filtering or encoding state;
- icons boxed in tinted or bordered rounded tiles ("icon badges"), including the app logo as a glowing tile;
- label pills for things that are not filters or states ("● Invoices"), pill clusters, hype badges (NEW, BETA, AI, FAST) without meaningful state, gradient text, glow halos, glassmorphism panels without a floating layer;
- oversized display type (80px+) for ordinary pages, giant-radius containers everywhere, emoji in headings/buttons/navigation, decorative terminal output and pseudo-telemetry;
- images without alt text, icon-only buttons and other controls without accessible names, skipped heading levels, autoplaying carousels, marquees and custom cursors;
- agent work-thought leaked into reader copy: first-person build narration ("I designed", "here's what I built"), making-of commentary ("this section showcases", "as an AI"); keep only the fact it was wrapping;
- the stock landing order (trusted-by, three feature cards, how-it-works, testimonials, pricing, FAQ, final CTA) and centered-everything layouts;
- the default indigo/violet-to-pink palette on a brand that does not own it;
- generic lightbulb logos and muddy orange/brass/brown default palettes; calling a generated palette "the product's own brand" is not evidence of user approval;
- boilerplate copy: "Unlock", "Seamless", "Elevate", "In today's fast-paced", em-dash floods, rhythmic triads, invented metrics or testimonials.

Replace ornament with structure: spacing and alignment for grouping, one clear type hierarchy, plain icons at text size only where they aid scanning, color reserved for meaning. The edit-time cues (`ui-accent-rail`, `ui-dot-marker`, `ui-glow-dot`, `ui-live-pill`, `ui-eyebrow-pill`, `ui-fake-status-label`, `ui-icon-tile`, `ui-stock-palette`, `ui-template-sequence`, `ui-pill-cluster`, `ui-badge-spam`, `ui-gradient-text`, `ui-glass-panel`, `ui-oversized-type`, `ui-rounded-excess`, `ui-emoji-ui`, `ui-fake-terminal`, `ui-missing-alt`, `ui-icon-button-name`, `ui-heading-skip`, `ui-auto-carousel`, `ui-custom-cursor`, `prose-ai-tells`, `prose-work-thought-leak`, `prose-unlock-cta`, `prose-vague-heading`) and `design_audit` findings (`repeated-heavy-left-border`, `decorative-dot-marker`, `animated-status-pill`, `eyebrow-pill`, `text-status-dot`, `icon-tile`, `missing-image-alt`, `unnamed-control`, `emoji-in-chrome`, `hype-badge-cluster`, `fake-terminal-decoration`, `slop-oversized-type`, `slop-heavy-radius`) flag these automatically.

## Final check
These are standing requirements even when the current prompt omits them. Inspect each reported policy cue at its actual location; fix a violation or document a specific user-brand/state exception through the existing quality assessment. A generic visual pass cannot clear an unaddressed cue. `aria-hidden` and a live conversation region do not make visible ornaments necessary.

Does the artifact solve the requested problem? Are claims supported? Are boundaries and failure behavior coherent? Did verification exercise the actual change? Optimize for contextual necessity, not visible completeness: every section, claim, widget and page must earn its place for this reader — apparent sophistication without necessity is the underlying failure mode (see the website necessity catalog in `references/patterns.md`). Avoid broad rewrites solely to satisfy a style heuristic.

Use available `artifact_check` with `operation:"ui"` for component source cues, then verify rendered and interaction states. For code, `code_quality` finds copies (`duplicates`, add `changed:true` for a branch), placeholders, leftovers and dead code (`slop`) and complexity hotspots; for prose, `code_quality` `prose` lists stock phrases with plain replacements and readability numbers. Findings are leads to inspect, not verdicts. See `references/patterns.md` for shared review evidence and code/prose checks.
