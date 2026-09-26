---
name: color-theory
description: Choose and critique UI palettes using perceptual color spaces, harmony, contrast and semantic meaning. Use for color relationships and accessible visual hierarchy.
---


# Color theory (for product UI)

Complements `colors` (which is the *engineering*: tokens, ramps, contrast checks). This one is the *judgment layer*: why colors work, which relationships to pick, and what each hue does to a reader.

## The perceptual ground rules (why "obvious" choices feel wrong)

- **People see lightness, not hue**. The first thing the eye sorts a screen by is value (light/dark); hue is the second sort, saturation the third. Two elements with different lightness will pop apart even if their hues are "similar"; two elements with the same lightness will merge even if their hues differ. Hierarchy bug in 9 cases of 10 = a lightness bug, not a hue bug.
- **sRGB is not perceived-linear, and HSL is perceptually broken**: HSL's "L" is not lightness to humans (mid-L is not visually mid-light — 50% L yellow is blinding, 50% L blue is near-black), and "S" is hue-dependent (S=100 shifts the effective lightness by hue). **Use OKLCH/OKLab for anything you compute** (ramps, "make this 10% lighter"): in OKLCH, changing `l` at fixed chroma*hue gives a uniform perceptual step (CSS `oklch()`; or the relative-luminance approximation). HSL is fine for quick one-offs, never for a ramp.
- **Value contrast carries the hierarchy; chroma is the spice**. A calm, professional UI = a narrow chroma range (mostly low-chroma neutrals) with 1-2 high-chroma accents. A loud, chaotic UI = high chroma everywhere. "Muddy" = mid-value + low-chroma + a warm bias (the gray-brown of tired palettes) — the fix is usually to *lower the value* of the neutrals (go cooler/darker) or raise the content's value contrast, not to add color.
- **Simultaneous contrast**: a color looks different on different backgrounds (a mid-gray chip pops on black, sinks on white; greens shift toward red next to red). Audit color decisions *on the real background*, never on flat chips (the palette-screenshot review is lying to you — a 5% value shift is invisible on white and obvious on #111).
- **Large saturated areas fatigue and dominate**. Full-stop saturated blocks (a #7c3aed hero, a green banner) read as alarm or gradient-slop; keep saturation high only in small areas (icons, dots, links, focus rings, CTA text) and let large surfaces be near-neutral.

## Hue relationships — when to use which

Pick the *relationship*, then the hues (the scheme is a constraint, not a decoration):
- **Monochromatic** (one hue, value+chroma steps): the safest for UI neutrals-with-brand (a bluish-gray surface set from your brand blue). Calm, unified; risk = flat/boring if the values don't separate (fix: wider value range, not more hues).
- **Analogous** (±30° on the wheel, e.g. blue→teal→green): soft, cohesive; great for *data series* and illustrations; weak for primary/secondary distinction (too close — a blue CTA and a teal "cancel" will blur at distance; if you need two distinct actions, go ≥90° apart).
- **Complementary** (180°, e.g. blue↔orange): maximum hue contrast; use for **the one accent** (CTAs, highlights, active states) against a neutral field — the classic and the right default for "one button matters." Two complementary elements at *equal weight* = a circus; one dominates, one punctuates.
- **Split-complementary** (the complement's neighbors): the calm version of complementary (blue vs yellow-green/red-orange) — good when pure complement feels aggressive.
- **Triadic** (3 × 120°): maximum hue distinctness with balance; use for **categorical data** (3 distinct series) or brand systems that need 3 voices — *not* for a full product palette (3 loud hues in one UI is noise). Tetradic (2 complements) is rarer still.
- **The product default**: neutrals (value ladder) + **1 brand hue** (identity, used for primary actions/links/focus) + **1 complementary accent** (where a second "pop" is genuinely needed: highlights, the "AI" moment) + **semantic hues** (success/warning/danger/info — see below). 5 hue families total; most screens show 2.
- Data-viz hues are a separate discipline (categorical palettes, 6-8 max + an "other" gray, `data-viz` skill) — don't import your UI accent as a data categorical (and never let the CTA color double as a data-series color: the eye will conflate "action" and "that metric").

## Hue meaning & convention (what each hue *does* to a reader)

Conventions are cross-cultural *and* learned (the SaaS logo wall is blue because a decade of "trust" branding trained it). Use the convention unless you have a *reason* to subvert (and then the UI must earn the relearn):

| Hue | Conventional load | Product use |
|---|---|---|
| **Blue** | trust, tech, calm, "safe" (the #1 SaaS hue; also Google/social) | primary brand hue for most products (it's the safe-and-correct choice, which is itself a decision — the anti-slop risk is the *generic* blue, not blue per se) |
| **Green** | go/success, money, health, nature | success states, "live", financial positive, health; (also: the 2020s "sustainable" brand green) |
| **Red** | danger/error/stop + urgent + passion (a *double* load — context decides) | errors, destructive, urgency (sales), record; **never a success color** |
| **Orange/Amber/Yellow** | caution/warning, attention, energy, "new" | warnings, pending states, badges ("beta"), highlight markers; yellow text is nearly unusable on light (contrast) — yellow is a *fill*, not a *text* |
| **Purple** | creative, premium, "AI" (the 2024-26 gradient-slop tell — purple+blue+pink mesh = instant "AI-generated" signal) | differentiation, premium, AI-features; use deliberately (a solid, confident purple ≠ a slop gradient — the tell is the *uncontrolled multi-hue mesh*, not the hue) |
| **Teal/Cyan** | calm, technical, clinical (between blue's trust and green's health) | medtech/data/dev; the #1 "calm blue" for dashboards |
| **Black/Neutrals** | premium, minimal, editorial | the surface language of most modern products (Linear/Apple/Vercel); *neutrals are the palette*, hue is the punctuation |
| **Brown/Warm neutrals** | organic, earthy, "human" | fintech-consumer (the "warmth against the bank blue" move), docs/reading; hard to make look *current* — do it with intent, not by accident (a "gray" that's actually brown = a muddy palette failing) |
| **Pink/Magenta** | playful, consumer, "for X" demographic signals | consumer/culture products; in B2B it reads as a mistake unless the brand owns it |

- **Cultural breaks**: red = good fortune/success in CJK markets (New Year red, financial *gains* red in mainland China — a trading app that ships red=loss to that market without an override is a real incident); white = mourning in parts of E. Asia (a "clean white" funeral-adjacent choice matters for the region you're selling into); blue/pink gender coding is *not* universal. If you ship regionally, the *semantic* hues (success/danger) get locale overrides — neutral surfaces don't.
- **Color is never the only signal** (this is color theory and accessibility joining hands): status = color + icon/label/text (the "red dot" without a label is a guess; the a11y floor is `colors`' rule, the design rule is the same: hue carries *emphasis*, meaning travels with words/symbols/glyphs).

## Building a defensible palette (the workflow)

1. **Start from the brand hue** (if there is one): derive a ramp by *perceptual* steps (OKLCH fixed hue, `l` 95→20, chroma capped so the mid-runs don't vibrate — a typical UI ramp has chroma in the 8-20 range for surface roles, higher only for the "intense" brand moments). If the brand hue is unusable for UI (a deep navy with no light side → the ramp is all "dark"; a neon lime that fails contrast everywhere → demote it to accent-only and pick a neutral-field companion).
2. **Neutral field first** (this is the 80%): 5-7 value steps (page bg, surface, surface-elevated, border, text-primary, text-secondary, text-tertiary) in the *temperature you want* (a cool blue-gray = tech/calm; a warm gray = human/docs — **pick the temperature deliberately; an "unsalted" pure #808080 is the default that reads as dead**). Dark mode: base #0f-#1a range *not* pure black (OLED crush + halation — see below).
3. **One brand hue for action** (links, primary CTA, focus ring, active nav) with its *own* light/dark variants (a single brand blue at one luminance fails: it needs a "text on light" run (~4.5:1 against the field), a "fill" run, an "on-dark" run (brighter, lower chroma), and a "subtle bg" run (the 50-100 tint for chips/banners)).
4. **Semantic set** (4 roles minimum: success / warning / danger / info): each role = a **pair** (strong for fill/icon, tint for bg) + a text-on-tint variant (the "red-700 text on red-50 banner" construction — the text is never the mid-tone on white; mid-tones fail 4.5:1 and it's the most common real-world contrast failure). Info = your brand hue by default (don't add a 5th family for info when the brand is already blue; add it only when brand ≠ blue *and* info must not read as "primary").
5. **The accent** (optional, 1): the complement of the brand for moments that must exceed the brand (AI-features, highlights, "live" indicators) — used in small areas only.
6. **Prove it**: the contrast matrix (every fg/bg pair at 4.5/3.0 — `colors`), the color-vision pass (protanopia/deuteranopia simulator on the *key screens*, the Okabe-Ito check on data, and the "remove hue, keep value — is the layout still readable?" test — if the value structure collapses, the palette is hue-led and broken), the dark-mode pass (below), and the **print/P3 check** (a #ff2d75 that's fine on an sRGB monitor is screaming on a P3 display — if you target Apple hardware, spec in `display-p3` or accept the shift — the "why is my brand pink on Mac and red on Windows" bug is a color-space declaration bug, not a taste bug).

## Dark mode (color is a *remap*, not an inversion)

- **Base is not black**: #0f-#1a with a slight hue-temperature matching the light theme (the dark theme is the *same* warm/cool family, darker — a cool-light product shouldn't go warm-dark). Pure #000/#fff pairs halate (vibrating text edges) and crush on OLED (gradients disappear into the black — add a ~2-3% lift to large flat dark fields on OLED-targeted products).
- **Elevation goes the other way**: light UI deepens shadows to elevate; **dark UI *lightens* the surface to elevate** (bg < surface < elevated, each ~4-6% lighter; use border + lightness, not shadow — shadow is invisible on dark, the hairline border is the elevation).
- **Desaturate ~10-20%**: saturated hues on dark *vibrate* (a #ff4444 chip on #111 pulses; the same red at chroma -15% sits). Full-sat accent colors read as neon in dark — tune them down, keep meaning.
- **Text is not white**: ~85-92% white (e.g. #e8eaed-class) as primary; pure #fff is for emphasis only (contrast against the too-light surface + halation).
- **Imagery**: photos render darker in a dark theme (the display is dark-adapted) → apply a slight brighten/contrast or a dark scrim; flat illustrations re-shade (a light-mode palette's mid-grays read as *black* in dark — re-map the illustration's palette variant, not just the page behind it).
- **The links/accent**: brand hue gets its *on-dark* variant (brighter value, slightly lower chroma) — the light-mode link blue at the same L is a "sinks into #111" bug (the "my link is invisible in dark mode" = the classic).

## Cases (what the good palettes actually do)

- **Stripe**: cool blue-gray neutrals + one blue (action/links) + semantic green/red/amber + a **purple reserved for the developer-docs brand** (the hue does *the job* of "this is the docs" — hue-as-function, not hue-as-decoration); the famous gradients live in *data-viz and marketing moments* (the "payments feel" illustrations), never in the dashboard chrome.
- **Linear**: near-monochrome (a #0f1014 dark base + 5 surface steps) + **one violet accent**, used with surgical restraint (focus, active, the AI moments); the entire "premium/developer" feel is value-discipline + 1 hue — the masterclass in "less is the palette."
- **Apple**: neutrals + one action blue + semantic minimal; color enters via *materials* (photos, the product render) and the OS accent (the user's system accent tints the UI — **a product can "let the user own the hue"**; a tasteful monochrome with an accent slot is the most defensible palette of all).
- **Figma**: the multi-hue logo is *mapped onto function* (blue=canvas/design, green=comment, purple=code/dev-mode-ish) — a brand that *is* multi-hue can afford multi-hue UI *if every hue is a job*; the lesson is not "use many colors" but "every hue needs a duty" (a hue without a job is noise by definition).
- **Notion**: warm near-black text/cream-ish surfaces, **the accent is the user's pick** (a hue slot the user fills) + semantic minimal; the "document" calm is a *temperature* decision (warm neutrals = paper = reading) — the proof that the neutral field's temperature is a design choice, not a default.

## The quick audits (when a screen's color is "off")

1. **Value-led test**: mute to grayscale — is the hierarchy intact? (no → you're carrying hierarchy with hue; rebuild the value ladder).
2. **The 4.5 hunt**: text failures are almost always (a) a mid-tone semantic text on white, (b) placeholder text, (c) text-on-image without a scrim, (d) the dark-mode link that didn't get its on-dark variant.
3. **Chroma audit**: how many pixels are above ~30 chroma? (more than the accents → the screen is shouting; desaturate the field, keep the accents).
4. **The temperature check**: are the grays warm/cool/neutral *by decision*? (mixed-temperature grays — a warm surface under a cool brand hue — is the #1 "can't name why this is off" cause).
5. **The hue-duties map**: can you name the *job* of every hue on the screen? (a hue without a named duty gets deleted; a duty without a hue is a missing token).
6. **Simulator pass** (protanopia + deuteranopia): the data still separates? the status is still readable (color+icon/label)? (the `colors` + `data-viz` hand-off).

## Detailed coverage

Color theory for product/web design — the perceptual spaces (why sRGB/HSL deceive, when to use OKLCh/Lab), hue relationships & harmony schemes (mono/analogous/complementary/triadic and which one to reach for), lightness-vs-saturation discipline (the two real levers), hue meaning & cultural convention (blue=trust, red=danger, and where that breaks), building a defensible palette (1 brand + neutrals + semantic), dark-mode color (not inversion), color diversity/deficiency (Okabe-Ito, simulator checks), and a worked case index (Stripe/Linear/Apple/Figma/Notion). Use when choosing or defending a product palette, diagnosing "why does this feel off", extending a brand into a UI, or fixing color in dark mode.
