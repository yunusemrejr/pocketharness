---
name: visual-composition
description: Compose screens using visual hierarchy, Gestalt grouping, spacing, alignment, balance and density. Use for layout critique or making a UI read clearly.
---


# Visual composition (making elements combine)

`ui-ux-principles` is the *why/reference* (hierarchy, reference examples); `fonts`/`colors`/`color-theory` are the element skills; `design-systems` is the token layer. This skill is the **join**: how the elements must relate to each other for a screen to read as one designed object instead of a pile. The core idea: **composition is constraint between elements** — most "feels off" screens have fine individual elements and broken *relations*.

## The Gestalt set — the actual laws, applied

- **Proximity (the cheapest grouping tool you own)**: things near each other read as one group. Cards are *spacing*, not borders; related controls 8-12px apart, unrelated 24-40px. The first fix for any "cluttered" screen: **remove a border and double the spacing** (the border was doing the grouping job the space should be doing — the result is calmer *and* lighter).
- **Similarity**: same size/weight/color = same kind (a bold number + a gray label read as one object; all-bold labels stop doing that). The kill-app: **two different components at the same visual weight** (a KPI number and a section header both 32px bold = the eye can't rank them) — break the similarity deliberately.
- **Continuation**: the eye travels along lines/columns/directions — use it to *route* attention (a column of items ending in the CTA; a row that points at the next section; nav → hero → proof in the scan path). A screen that fights its own continuation (the route bends twice before the action) is a screen that loses skimmers.
- **Closure**: the eye completes near-closed shapes — borderless tables, gap-separated list rows, implied containers all work *because* the browser fills the gap. The over-use: so many implied containers the groups stop reading (3+ implied groups per column and the "where does one group end" is the tell).
- **Figure-ground**: exactly **one figure should pop per view** (the hero number, the product shot, the CTA row). Two popping elements = a tie = both lose (the #1 composition bug, measured by the "point to the screen, where does your eye land?" 3-second test — two answers is the failure).
- **Common region**: a container/panel = a set (the drawer, the settings section). Panels are *expensive* visually (every panel is a figure against the ground) — **the test: could proximity+alignment do this grouping for half the weight?** (usually yes; panels earn their keep for *persistent* groups, not temporary ones).
- **Symmetry & order / preattentive attributes**: balanced default, *one* deliberate break per view (the off-grid accent stat, the tilted image, the single full-bleed row — the "one break" is the interest; five breaks is noise). Preattentive (size, weight, color, orientation) is read *before* cognition — you get **one** preattentive pop per view (use it for the rank-1 element; the rank-2 must win only with the read, not the glance).

## Hierarchy in practice (the mechanics)

1. **One primary, one secondary, everything else is support.** Assign before designing: what is the ONE thing this screen is for? (a KPI, a CTA, a document title) Everything is weighted relative to it.
2. **The type scale is the hierarchy engine**: 3-4 steps per view, ratio 1.2-1.33, **a size step = a rank step** (no half-steps, no 7-step scales on one screen — the `fonts` skill for the scale itself). The number that matters is *disproportionately* bigger than the one that matters less (the "27%" KPI is 2-3× its label, not 1.4× — subtle steps read as "no hierarchy" to a skimmer).
3. **Whitespace is an element, not the leftover**: measure the ratio — generous/editorial (margin ≈ 2× the content spacing), comfortable/product (≈1:1), dense (≈0.5:1). **Pick the ratio by device and task** (a landing page at 0.5:1 is cramped; a data table at 2:1 is absurd), then hold it consistently (the "why does this section feel different?" bug is usually a *ratio* slip, not a value slip — 13px between one pair in an 8pt world is visible).
4. **Vertical rhythm**: a repeating spacing interval (the 8pt grid, `design-systems`) is the metronome — sections breathe at the same interval; a section that breaks the rhythm *is* a section break (the 96-128px "chapter" space between major sections vs the 24-32px "paragraph" space within them — the two-tier rhythm is the layout's grammar).
5. **The scan pattern is the layout**: desktop skims in an **F** (top-left is prime real estate: logo→nav, then the first two lines, then down the left rail) — *land the value line top-left* and the CTA at the end of the scan; hero-landings run a **Z** (logo→nav→visual→CTA at the exit corner) — the CTA at the Z's end (top-right after the visual) is why the "sign up" button lives where it does. Mobile collapses to a single downward stream — **one column, one action, no "end of scan" to work (the sticky bottom CTA replaces it)**.

## Combining the elements (the pairings that get it wrong)

- **Type + image**: the image *anchors* a column (text aligns to the image's edge, never floats center-stage over it); the image aspect is set *by the slot* (a 4:3 photo in a 16:9 slot is a design decision, not an unfortunate crop — crop to the slot, subject-safely: face-safe = focus on the eyes, `object-position` is the tool); the classic pair = **copy left / visual right (desktop), stacked on mobile with the copy first** (skimmers read left→right, top→bottom — put the sentence they need to leave on before the pretty thing they can't); text over image **always through a scrim** (a 30-60% gradient from the text side; raw white-on-photo fails contrast *and* it's the #1 amateur pairing — no exceptions, "it looks fine on my screen" is the luminance of the room, not of the photo).
- **Image + color**: the page either **derives from the photos** (pull 2-3 hues from the imagery into the palette → page and photos agree, the "designed" feel of most editorial work) or **treats them neutral** (grayscale/monochrome photos + one saturated accent → the "premium editorial" look). Mixing (a warm photo set on a cool UI with an unrelated accent) is the "feels off" that has no other name — **decide the relationship explicitly; don't let it happen by accident**.
- **The three elevation languages — pick one per product**: (a) **flat + space** (no shadows, no borders; groups by spacing and background *value* steps — the Linear/Apple mode), (b) **hairline borders** (1px, low-contrast borders define everything; the dark-mode-native choice — `colors`), (c) **soft shadows** (the Material/inset-card mode; shadows need the light theme *and* a consistent level scale 1-3). **Mixing all three on one screen is a specific tell** (a shadowed card + a bordered input + a gradient border + a glow = four languages, four designers). One language, consistently, is ~half of "this looks professional".
- **Grid + asymmetry**: 12 columns (the web standard), gutters 24-32px desktop, page margins = the frame, **max-width container 1200-1440px** (text measure 65-75ch — `fonts`); balanced 6/6 splits are neutral, **asymmetric 8/4 (or 7/5) is where the interest comes from** (the "editorial" feel is an asymmetric split + a baseline alignment between the columns); **break the grid at most once per view** (one full-bleed band, one off-grid stat) — the break is the event; two events is a story, five is chaos. Full-bleed sections alternate with contained ones = the pacing (the "rhythm of the page" is literally this alternation).
- **Balance is a scale, not a mirror**: visual weight = size × contrast × saturation × detail — a small red dot balances a large gray block. Left-heavy pages (the common case: a big product shot left) take the counterweight on the right (a cluster of light elements, a tall type stack, whitespace *as* the weight). A page balanced by *mirror* (identical halves) is symmetric = formal = often boring; the **asymmetric balance (unequal weights, equal tension) is the "designed" look** — test it: blur the screen (or squint) — if the eye rolls to one side, the scale is off.
- **Rhythm & variation**: alternate heavy sections (a full-bleed visual + big type) with light ones (type-only, generous space) — the *alternation* is the pacing (a page of all-heavy is exhausting, all-light is drift; the 10-second scroll should feel like a pulse); within a section, **repetition of the interval** (the items at 24px, the groups at 64px, the sections at 96-128px) — the regularity is what the irregularities get measured against.
- **Density by device/role**: tool/dashboard = dense (compact spacing, real borders, visible structure — the `desktop-ui` patterns), landing/marketing = roomy (the whitespace *is* the premium signal), docs = mid (the 3-column code:prose ratio, `ui-ux-principles`). A pattern is only right at its density — a landing-card grid on a dashboard and a dashboard-table on a landing page are both "wrong density", which reads as "amateur" before anyone names it.

## The named failure combos (the diagnosis list — symptom → cause → fix)

- "It has all the right parts but feels flat" → **no preattentive pop** (everything mid-weight: the figure-ground is a tie) → pick the one element, 2× its scale, everything else down a step.
- "Feels cluttered but nothing removed" → **borders doing grouping jobs** (proximity was never the tool) → delete a border class, double the spacing of the removed group.
- "Feels busy / loud" → **too many preattentive pops** (3+ bolds, 2 saturated regions, 3 sizes on one line) → one pop, the rest support; the `color-theory` chroma audit (how many pixels are >30 chroma?).
- "Two things fight for attention" → the tie (KPI and header at one weight / hero and CTA row both popping) → break the similarity (one goes up a scale step, one down).
- "The image looks pasted on" → unanchored (not aligned to a column edge, fighting the grid) → align its edge to the grid; set the slot's aspect; scrub if it's over type.
- "White-on-photo is unreadable *sometimes*" → no scrim (the photo's luminance varies) → the gradient scrim (30-60%), always.
- "Elevation is inconsistent" → mixed languages (shadow + border + gradient-border on one screen) → the 3-way pick above, one, everywhere (tokens: `design-systems`'s `--elevation-*`).
- "Sections feel arbitrary" → no vertical rhythm (13px, 28px, 55px…) → the 8pt metronome (the 24/64/96-128 two-tier rhythm); sections land on the grid.
- "The page feels top-heavy / bottom-heavy" → the balance scale (squint test) → counterweight (whitespace counts as weight — *add space on the heavy side's opposite* is the last-resort fix that's almost always cheaper than moving elements).
- "Everything looks the same width / everything is centered" → **centered everything** = formal (centering is for *statements* — hero, single CTAs, error pages — never for a whole page; left-align is the working default, center is the event).
- "Different card types in one grid row" → mixed kinds in one group (a stat card next to a CTA card next to a quote card in a 3-col row) → one kind per row (the *kind* is the group; different kinds = different rows/sections), or make them all the same shell (same padding, same header slot, same footer slot — the `design-systems` component discipline).
- "One element looks wrong and I can't say which" → the 3-second test × 3: squint (balance), grayscale (value structure — `color-theory`'s test), and the headers-only read (the composition is carrying a story or it isn't — the `storytelling` handoff).

## Cases (what good composition is doing — study these, from the links)

- **Stripe docs** (the density masterclass): a 3-col × 2:1 code:explanation grid, a fixed left nav rail, section breaks as *space* not borders (the two-tier rhythm — 24px within, ~96px between), the density that reads *calm* because every unit is on the 8pt metronome and the one figure per view is the code block (the pop is monospace + the background step).
- **Linear home** (the restraint masterclass): dark field, one accent, an almost-total *absence* of ornament — the composition is asymmetric split + type scale + one product-screenshot figure; it proves that **when the grid and the scale are disciplined, nothing else is needed** (the anti-`anti-ai-slop` reference: zero gradients, zero blobs, highest polish per pixel on the web).
- **Apple product pages** (the pacing masterclass): full-bleed alternating with contained, one beat per section with *the section is the sentence* (headline = the clause), the whitespace ratio at its editorial max (the "premium" signal is *mostly* the 2:1+ margins), the motion as the beat-marker (`motion` skill) — the page is a film strip, the scroll is the timeline.
- **Awwwards SOTY-class work** (the experimental edge — study for technique, not for template): the experimental layouts still pass the screen tests (one focal per screen, the nav disappears until you want it (a hover/reveal chrome — the "clean" award tell), the *scroll is the narrative* (`storytelling`), a deliberate grid-break per view) — the award sites are composition *theories*; ship the discipline (one focal, a rhythm, a break), not the gimmick (unless the product is the gimmick — then the gimmick is the product, which is a different brief).
- **Mobbin (real product flows)** (the ground-truth density library): the settings screens are where density is specified (the form groupings, the row heights, the section spacing of shipped products), the onboarding flows are where pacing is specified (the beat-per-screen, the progress, the skip) — **when in doubt about a density, look at what shipped, at that device, in that category** (it is the market's vote, not a style preference).

## The 60-second composition audit (any screen)

1. Point test (3s): one focal? (no → the pop assignment above).
2. Squint test: balance level? (rolls to a side → counterweight).
3. Headers-only: a story? (no → `storytelling`).
4. Grayscale: value structure intact? (no → `color-theory`'s value ladder).
5. Count the spacing values: 2-3 families or 9? (9 → the 8pt metronome; the two-tier rhythm).
6. Elevation: one language? (no → the 3-way pick).
7. Borders: which ones are doing grouping jobs space should do? (delete, double the space).
8. Image: anchored? scrimmed where over type? aspect set by the slot?
9. Cards/grids: one kind per row? on-column?
10. Density: right for the device/role? (dashboard ≠ landing ≠ docs).

## Detailed coverage

How elements combine into screens that read as designed — the Gestalt laws applied (proximity, similarity, continuation, closure, figure-ground, common region), hierarchy in practice (one primary element, the scale steps, whitespace as an element), the three elevation languages (flat+space vs shadow vs hairline — pick one), grid systems (12-col, gutter/margin/max-width, asymmetry, breaking the grid deliberately), type+image and image+color pairings, balance as a scale not a mirror, rhythm & pacing across sections, density by device, and the named failure combos with their fixes. Use when assembling any layout (landing, dashboard, marketing page) or when a screen "has the right parts but feels off" — this is the diagnosis skill for combinations, not for individual elements.
