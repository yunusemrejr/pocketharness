---
name: frontend-design
description: >-
  Front-end design — making a browser UI read as one coherent system. The five layers (structure, composition, type, color, motion) and the unifying principle; the finite-set rule (a page's identity is its finiteness: one spacing scale, 3 type roles, 2-3 colors, 1 radius, 2 shadows); layout (containers, the 12-col grid with gutter consistency, section rhythm, whitespace-as-design); type as structure (4 roles, line length/height, pairing); color role assignment (90/10, dark as a system); component specs (button hierarchy, one card spec, input/icon specs); responsive (priority-first, few big jumps, 12→8→4); the emphasis rules (one emphasis per element, vary the kind — the anti-monotony that keeps it from reading as AI); and the self-audits (squint test, the 5-second statement, the remove test). Use when designing or redesigning any web page, when a screen "lacks polish/consistency" and you need the names for why, or as the capstone over the other UI skills (visual-composition, fonts, colors, motion).
---

# Front-end design (the capstone)

The other UI skills are the organs — `visual-composition` (composition), `fonts` (type), `colors`/`color-theory` (color), `motion` (motion), `design-systems` (scale/consistency), `web-patterns` (shapes). This skill is the *organism*: the discipline that makes a page read as **one system, one voice, one hand**. The failure mode it exists to kill: every part is technically correct and the page still looks *assembled* — the generic "competent" look.

## The principle: identity is finiteness

A coherent page is *finite*: a small, repeated set of decisions that the eye learns. The eye recognizes a system by its **repetition** — the same spacing scale, the same three type roles, the same accent doing the same job everywhere. Incoherence is *excess of exceptions*: a fourth radius here, a new shadow there, a "special" spacing for one section. The test: **could a stranger list your page's set?** (three type sizes, two spacings, one accent usage, one radius) — if the answer is a list, you have a system; if it's "honestly it varies," you have a collage that happens to be polished.

## The five layers, and the one rule per layer

1. **Structure (IA → layout)**: the grid + container + section order carry the *logic*; `web-patterns` owns the shapes, this layer owns the *skeleton*. One container for the page (1200–1440 on marketing; fluid-with-max for apps), one gutter used **everywhere** (gutter consistency is what reads as "coherence" before any other thing), 12 columns collapsing 12→8→4 at the real content breaks (not 12→11→9 — the intermediate column count is the half-jump that reads as "mobile-ish desktop").
2. **Composition**: hierarchy first — `visual-composition`. The rule for the *whole page* (vs. one element): the **one emphasis per screen, one job per emphasis**. The hero gets scale; the CTA gets color; the feature gets whitespace — *never two elements fighting for the same emphasis type in the same view.*
3. **Type (type is structure)**: four roles, each a fixed size+weight. **Display** (the one statement: 48–72, tight 1.1–1.2, the distinctive face), **Heading** (section: 24–32, 1.2–1.3), **Body** (reading: 16–18, **1.5–1.7**, 60–75ch measure), **Label/caption** (metadata: 12–13, the letter-spaced small-caps territory, 0.5–1.5px tracking — tracking only on *short* uppercase, never on body). The pairing: one *distinctive* + one *neutral* (`fonts`); the display's voice is the brand, the body's job is invisible. **Line-height tightens as size grows** (the display at 1.6 is the amateur tell); **the measure is the body's most important number** — the 90ch paragraph is the unreadable-page bug.
4. **Color (roles, not a palette)**: the palette is 90% neutral (the surface/ink scale) + 1 accent that **acts** (links, primary CTA, focus, the one highlight) + the state colors (success/warn/danger — `design-systems`: they mean the same thing on every screen). The **90/10** is structural, not stylistic: the accent *everywhere* is the neon-sign effect (no hierarchy survives). Dark mode is a **system, not an invert** (`color-theory`: the surface/ink elevation model rebuilt, the accent's *luminance* retuned, the pure-black-pure-white kill). If color does two jobs on one screen (accent *and* section-differentiator), the accent has lost its meaning.
5. **Motion**: `motion` — one system of duration/easing (the 200/300/450 trio, the standard ease-out family), the *compositor-only* rule, the reduced-motion *branch*. The page level: one entrance narrative (the page assembles once, on load — then it's *calm*; a page that keeps animating after load is a page that isn't finished).

## Layout: the section rhythm

The page is a **rhythm of densities**, not a stack of boxes: hero (spacious, few elements) → the dense band (the feature grid / the proof) → the spacious band (the statement) → the dense (the CTA block). Alternating density is what makes a long page *read* — five evenly-dense sections are the wallpaper (the eye has no valleys to rest in). The vertical scale: pick **two** section paddings (e.g. 96/128 on desktop, 64/96 mobile) and use only those — the "random 87px" section gap is the un-system tell. The hero's height: **the content decides** (the one-statement + the one-action hero is often *short*; the 100vh-empty hero is the scroll-punishment the user punishes back).

## The components: one spec per class

- **Buttons**: 3 tiers, max. Primary (accent fill — *one per view*, the view's single action), secondary (outline/surface — the alternative), quiet (text/ghost — the tertiary, the inline). One height per tier (the 40/48 desktop / 44 touch minimum — `desktop-ui`), one radius. A fourth tier or a second primary is the hierarchy breaking.
- **Cards**: **one card spec** (radius, border *or* shadow — not both, padding, the internal type roles). Emphasis happens by *content* (an accent bar, an upward-traffic metric) — a "featured card" that gets a *new shadow + a scale + a border* is the decoration stacking (the card does the emphasis that should be the content's). `hover` on cards: the *quiet* one (the 2px lift or the token bg-shift — `motion`), never the gradient-border neon.
- **Inputs**: the spec from `web-patterns` (label, focus ring 2px accent, the error inline). The input's focus ring is where the accent *must* live (the keyboard user's map of the page).
- **Icons**: `svg-assessment` (24-grid, currentColor, one stroke weight, the row-test). The icon set is *one family* — one downloaded-from-elsewhere icon is a fingerprint of the un-system.

## The emphasis rule (the anti-monotony doctrine — this is what keeps it from reading as "AI")

AI-assembled UI is monotone *within* its competence: everything bold-ish, everything equally spaced, one emphasis type (weight or color) for everything, no element that's *quiet*. The human tell is **contrast + variety of emphasis**:

- *Scale contrast*: the page needs at least one display-vs-caption *juxtaposition* (the big number next to its 12px label — the hierarchy is visible *between* them, not in either alone).
- *Variety of emphasis type*: scale for the statement, color for the action, whitespace for the feature, weight for the key phrase — **the same emphasis type twice in the same view is the signal** (two colored elements = two "most important" things; neither is).
- *Quiet is a tool*: the *reduced*-presence element (the 60%-opacity caption, the borderless zone, the unstyled meta) is what makes the loud things loud. The all-60%-or-louder page has no baseline (the value of the accent is defined by the neutrality around it — `colors`' "the neutral *is* the design").
- *Asymmetry over symmetry-by-default*: the centered-hero-everywhere is the template tell; the left-aligned statement with the margin breathing is the editorial answer (`visual-composition`'s asymmetric rule, applied page-wide).

## Responsive: priority-first, few jumps

- *Mobile-first means priority-first*: the mobile layout **decides the content order** (what a thumb needs first survives to desktop as the left/first column) — the desktop-priority-reflowed-down is the "condensed" tell (sections that fight for order at 390px).
- **Three breakpoints, big jumps**: ~640 / ~1024 / ~1440 (or 768/1024/1400). The 6-breakpoint design is the pixel-panic; the jumps are where the *content* changes (the columns 12→8→4, the nav → drawer, the grid 3→2→1), not where a margin feels 4px wrong.
- Nav collapse: content-nav above 768, the drawer below (`web-patterns` bottom-nav for the app-shell cases); the *action* (login, primary CTA) stays visible at every width (the collapsed-nav that hides the only CTA is the mobile conversion bug).
- Touch targets ≥44 everywhere (`desktop-ui` via the web: the 32px web button is the mobile fail); the **hover-state on touch = the active/tap state** (the hover-only info is invisible on touch — `@media (hover: hover)` gates).

## The texture budget (where "premium" lives)

Premium is **restraint with one flourish**, not ornament density. The page budget, max: ≤2 radius values (e.g. 8/16, or 12/full), ≤2 shadow levels (or borders-only — the *all-border, no-shadow* and the *all-shadow, no-border* are both fine; **mixed is the messy one**), ≤1 gradient (and then as a *background* accent, not a text-fill — the gradient-text is the 2023 tell; the gradient-accent-bar is the modern one), ≤1 decorative element per screen (the texture `css-battle`-style, the oversized type, the single illustration). Each flourish *earns the other restraints* by being singular — five flourishes is the casino.

## The self-audits (run before shipping any screen)

1. **The squint test** (the hierarchy survival): squint until the text is gone — can you still see the *order* (what's first, what's second, where the action is)? If the screen is a flat gray field when squinted, the hierarchy is in the *text*, not the *composition* — the design fails with the reading gone.
2. **The 5-second statement** (the communication): five seconds, then answer — what is it, who's it for, what do I do? Three answers from a stranger, pass; one of three, the hierarchy or the copy is failing (`copywriting` + the design are co-responsible — the design can't carry a copy failure and copy can't carry a composition failure; fix the *design* first if the answer is "I don't know what it is," the copy first if it's "I know what it is but not why I'd care").
3. **The remove test** (the restraint check): remove one element (a badge, a border, a third stat, a second button) — if the screen is *better*, it should be gone. The page that survives N removes is the page that's finished at N+1 elements. The AI-assembled page usually *improves* under the remove test — that delta is the excess.
4. **The finiteness test** (the system check): can you list the set in 10 seconds (type roles, spacings, radius, accent jobs)? Yes → system. No → pick the finite set *first*, then apply — the "design" work is mostly the *decision of the set*, the "making" is the application.
5. **The platform-honesty test**: does the web app look like a web app (the native affordances — the text-selection, the right-click, the URL, the tab — `desktop-ui` for the desktop-framed cases) or is it fighting the platform (the fake-titled web app, the mobile-frame on desktop)? The platform-honest UI is the one that never makes the user notice the platform.

## The anti-list (the assembled-tells, the things that say "AI made this" even when every part is correct)

- 5+ type sizes with no named roles (the "it's a scale" without the *roles* is the number-soup).
- Two colored elements in one view (the accent does two jobs — the most common AI failure; `colors`' rule).
- Border + shadow + bg-tint all on the same card (the decoration stack — the "emphasis" is the CSS, not the content).
- Everything centered (the centered-everything is the template; the asymmetry is the hand).
- Equal-weight everything (no display/label *juxtaposition* — the monotone, the remove-test-failer).
- A hero with three CTAs (the view has no opinion — one action per view, the hierarchy rule applied to CTAs).
- The gradient text, the glassmorphism pane with no backdrop reason, the animated blob (the 2022–2024 stock moves — `anti-ai-slop`'s visual tells).
- The generated-UI ornaments: colored left rails on cards, dots before labels and chips, icons boxed in tinted rounded tiles, label pills for non-filters (`anti-ai-slop` "Never produce" — never write them).
- A "feature grid" of 6 identical cards (the 3×2 of the same-shape = the no-opinion grid; the varied card *sizes* per the content's importance is the editorial answer).
- Mobile: the desktop-priority reflow (the 3-column grid that becomes a 0.5-column soup), the hover-only affordances, the <44px targets.
- The 100vh-empty hero, the 6-breakpoint pixel-panic, the hover-gradient-border neon.
- The *decorative* element with no job (the blob, the gradient, the oversized type that says nothing — every flourish needs a *job* or it's the casino).

The one-line version: **the page is a finite set, applied with rhythm, with one emphasis per view and one job per element; if a stranger could list the set and a squint could see the order, it's designed — if the removes make it better, it isn't finished.**

## Avoid default component filler
Start with the project's existing components, tokens and real content. Do not add decorative LIVE pills with blinking dots, fabricated metrics, redundant badge rows or motion that only advertises activity. Define font roles, color emphasis and content alignment as one system; verify long content and loading/error/empty states. Use available `artifact_check` with `operation:"ui"` for source cues, then verify the rendered result and actual controls. Keep functional status and intentional brand treatments when justified by the task.
