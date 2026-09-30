---
name: ai-design-slop
description: Required before UI/UX/GUI design or implementation: preserve project identity, avoid generic purple-gradient SaaS and cream/cursive templates, validate real content, interaction and visual hierarchy.
---

# Preventing AI design slop

Design from the actual product, audience and task. Preserve the established project identity and intentional user choices. A color, font or component alone is not a defect; the defect is copying a stock visual formula without a reason it serves this screen. Do not replace a functioning identity with a different template to satisfy a heuristic.

## Discovery before styling

Inspect existing screens, tokens, components, real content and the user's references. Name the screen's primary job and required states. Reuse the current design system unless the requested change or observed defect justifies altering it. For a new product without an established identity, derive choices from its subject, use and audience; do not invent a brand and then claim it was supplied.

Common template combinations to question:

- Gratuitous indigo/purple/pink gradients, glow halos, glass panels, pill above a centered hero and identical icon cards.
- Cream/terracotta backgrounds, oversized cursive or italic serif display, ornamental hairlines and editorial texture unrelated to the content.
- Boilerplate SaaS section order, vague promises, fabricated metrics, testimonials or customer logos.
- Colored left rails, decorative status dots, animated LIVE/BETA pills or tinted icon tiles that encode no real information.
- Oversized ordinary-page headings, excessive corner radius, emoji chrome, decorative terminal output and motion without a user or narrative purpose.

Several of these together should prompt inspection, not an automatic redesign. An existing purple brand, useful card grid, appropriate serif or actual live state can remain. Record a specific brand/state exception for a reported cue rather than treating a generic visual pass as clearance.

## Implementation and review

1. Use real content and real states. Label uncertain/placeholder content; never present invented numbers as evidence.
2. Give repeated elements a job: selecting, grouping, navigating, filtering or conveying state. Use spacing, alignment and typography for hierarchy before adding decoration.
3. Keep text legible at the actual viewport and contrast; preserve semantic headings, accessible names, keyboard/focus behavior and reduced-motion support.
4. Run the swap test: if changing only the logo/headline makes the screen suitable for an unrelated product, inspect which content/layout choices are generic and repair the observed weakness.
5. Verify the changed user journey and rendered empty/loading/error/disabled states. Screenshots establish appearance; interaction needs browser or equivalent input evidence. If the environment lacks visual tools, report the gap and use DOM/geometry/contrast checks where available.
6. Make one focused pass with `anti-ai-slop`, `ui-antipattern-review` or `accessible-interaction-design` as relevant. Inspect each concrete reported cue once. Repeat only after an actual change, new failure or unresolved concern; reviews of an unchanged artifact add no evidence.

Use available native `pocket kit` quality and browser tools after checking current help. JEV findings are advisory leads tied to the changed artifact and user context; they do not prove genericity, correctness or accessibility. Resolve a cue through the actual source/rendered state, preserve specific intentional exceptions and avoid duplicate requests with identical context.

Deliver the working interface with its verified behavior and material remaining limitations. Do not leak agent build narration into reader-facing copy or claim visual review, keyboard testing or performance measurement that did not occur.
