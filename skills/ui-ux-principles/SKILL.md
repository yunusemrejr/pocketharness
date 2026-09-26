---
name: ui-ux-principles
description: UI/UX principles with concrete reference examples from the web (Stripe, Linear, Vercel, Figma, Awwwards winners) — hierarchy, density, motion, states, accessibility. Use when designing or improving interfaces, picking patterns, or when a UI feels generic/slop.
---

# UI/UX Principles

## Core rules

1. **One primary action per screen.** Everything else is secondary styling. If two buttons look equal, the user can't tell which to click.
2. **Hierarchy through 3 levers only:** size/weight, space, position. Color is the 4th lever and breaks for color-blind users.
3. **Density is a choice, not a failure.** Two registers: calm (Notion, Linear — generous whitespace, one thought per screen) and dense (GitHub, terminal tools — rows, tables, keyboard-driven). Pick one per product and stay coherent.
4. **Spacing systems:** 4pt base (4/8/12/16/24/32). Inconsistent 13px vs 17px gaps is the #1 "unpolished" tell.
5. **Motion has a job:** confirm an action (100–300ms, ease-out), show continuity (panels slide from where they came), never decorate. `prefers-reduced-motion` respected.
6. **States are features:** every interactive surface needs empty, loading (skeleton > spinner when structure is known), error (with a recovery action, not just "something went wrong"), partial/disabled, and success.
7. **Feedback < 100ms** for anything the user did; optimistic UI for likely-successful local-state ops, revert on failure with the error visible.
8. **Accessibility is not a pass/fail checkbox:** real keyboard flow (tab order matches visual), visible focus, accessible names, 4.5:1 contrast (see `colors`). Test one flow with Tab only.
9. **Progressive disclosure:** show the common case; hide advanced options behind "Advanced"/overflow. Linear's settings and Vercel's deploy panel are the reference.
10. **Copy is UI:** error messages say what happened, why, what to do next — who/what/where nouns, no "Error 0x1F32 occurred. Please try again."

## Reference examples (what to study, specifically)

| Product | Steal from it |
|---|---|
| **Stripe (stripe.com/docs)** | Task-first docs: user goal → working code in < 1 minute; test credentials inline; API reference separated from guides; every page answers one question |
| **Linear (linear.app)** | Calm density: one concept per page, keyboard + mouse parity (commands, shortcuts everywhere), restrained motion that confirms rather than decorates, consistent 4pt spacing discipline |
| **Vercel (vercel.com/docs)** | Path-based onboarding: framework-specific quickstarts with equivalents for CLI vs dashboard shown side by side; product-led pages that show the tool instead of describing it |
| **Figma (figma.com)** | Playful precision: brand personality without breaking density; bento layouts that still have one focal point per panel |
| **Apple (apple.com)** | Restraint: near-black/white, one product = one promise per screen, typography doing the layout work |
| **Awwwards 2025 Site of the Day winners** (e.g. Terminal Industries — industrial storytelling, Vercel Ship 2025 — dev-event UX, OPTIKKA — image-led commerce, Hyperbolic — experimental-interaction tech brand) | Interaction patterns and art direction; study the *decision* behind a section, don't copy the pixels. Pair with **Mobbin** for real product flows (onboarding, checkout, settings) — Awwwards is brand experience, Mobbin is product UX |

How to mine an example: document hero promise → nav → proof → features → CTA → friction; then rebuild ONE section at a time (pricing table, empty state, onboarding), never the whole aesthetic.

## Anti-patterns to reject

Modal-on-modal, forced onboarding tours, infinite scroll for lists under 50 items, dark patterns (confirmation horror, pre-checked upsells), decorative-only animation, emoji as icon system, fake social proof, "dashboard" that's a wall of identical cards, toast spam (one toast per state change = noise), pill-everything, hype-badge rows, gradient headline text, glass-by-default, fake terminal decoration, autoplaying carousels and custom cursors.

## Pre-ship checklist

[ ] One obvious primary action · [ ] All 5 states exist for new flows · [ ] Tab-key walkthrough works · [ ] Contrast ≥ 4.5:1 (text) / 3:1 (controls) · [ ] 4pt spacing audit · [ ] No lorem/fake data anywhere · [ ] Narrow (360px) + wide (1440px) verified · [ ] Motion ≤ 300ms or off under reduced-motion · [ ] Every image has alt, every control has a name, headings run h1→h2→h3 · [ ] No pill/badge/gradient/glass/emoji/terminal ornaments.