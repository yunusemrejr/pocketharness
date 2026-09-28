---
name: ai-design-slop
description: "Mandatory before any UI, UX or GUI work: the two live generations of AI design slop (indigo/purple SaaS, cream/terracotta editorial), the full tell checklist, and the nine-step procedure that avoids them."
---

# Preventing AI Design Slop


AI design slop is the default output of a model asked to design with no constraints: the statistical median of its training data, rendered as a page. Two generations of it are live right now, both diagnosed below, plus the mechanism that produces both and the procedure that avoids it.

## What slop actually is

An unconstrained model doesn't design — it averages. Asked for "a SaaS landing page" with no other input, it outputs the statistical median of every SaaS page in its training data: the most probable tokens, not a considered choice. Adam Wathan picked `indigo-500` as Tailwind UI's neutral demo accent in 2019, with no design rationale behind it. Tailwind's popularity meant billions of tokens of `bg-indigo-500` code entered training corpora. Models learned it as ground truth — "buttons are purple" — the same way they learn any other high-frequency pattern.

This compounds. AI-generated sites ship to the live web, get scraped, and become training data for the next generation of models — a feedback loop where the model is now training partly on its own output, amplifying the original bias each cycle. The same mechanism explains both generations below: a genuinely new aesthetic gets adopted, floods the training distribution once enough people ship it, and becomes the new median — indistinguishable slop under a different palette.

## Generation 1: indigo/purple SaaS

The dominant look since roughly 2023, still the default any unprompted agent reaches for.

| Element | What it looks like | Why it's a tell |
| --- | --- | --- |
| Accent color | Indigo-to-purple or purple-to-pink gradient (`#6366f1`–`#a855f7`–`#ec4899`) | Traced directly to Tailwind's `indigo-500` demo default; now "the single loudest AI tell," per design critics tracking the pattern |
| Typeface | Inter, or Space Grotesk / Geist paired with Inter | "The safest possible answer" — signals no typographic decision was made at all |
| Background | Near-black or deep navy, permanent dark mode | Defaults to dark because dark hides low-contrast text; body copy frequently fails WCAG AA contrast as a result |
| Cards | Three or four identical rounded cards in a row, thin-line icon centered at the top, soft shadow, sometimes a colored top or left stripe | Copied from Tailwind grid tutorials until it became the model's learned default for "feature section" |
| Ornaments | Colored glows/box-shadows behind headlines, badge pill sitting directly above the H1, all-caps eyebrow labels, emoji used as section icons | Pure decoration with no informational role — the badge-above-H1 and glow-behind-headline pairing shows up across almost every AI-generated SaaS mock |
| Numbers | A stat-banner row of big invented figures ("99.99% uptime," "2M+ deploys") and fabricated customer logos | Filler presented as evidence; nothing behind the numbers |
| Copy | "Build faster. Ship smarter." — short punchy fragments that name no real capability | Weightless by design: could be pasted onto any competitor's page unchanged |

## Generation 2: cream/terracotta editorial

The reaction to generation 1, now itself a cliche — adopted partly because it reads as "the opposite of AI slop," then flooded the training distribution the same way indigo did.

| Element | What it looks like | Why it's a tell |
| --- | --- | --- |
| Background | Beige/cream/ivory (`#F4F1EA`-family), warm off-white instead of pure white | Signals "considered editorial," borrowed wholesale from a handful of design-forward brand sites (Anthropic's own included) until every AI output converged on it too |
| Accent | Rust, terracotta, burnt orange | Swapped in as the anti-purple; equally a default the moment it's applied without a subject-specific reason |
| Typeface | Oversized serif display, often italicized, tracked-out all-caps subheads with extra letter-spacing | "Considered typography" as a costume — the serif is chosen for its vibe, not because it fits the content |
| UI chrome | Hairline rules, ticker-style text bars, rounded-rectangle outlines with a neon or soft glow, desaturated mid-century accent hues | Decorative texture standing in for actual information hierarchy |
| Overall effect | "Tasteful, slightly askew" — deliberately imperfect in a way that itself became a pattern | Kyle Chayka's framing: once informed viewers recognize the formula, the page reads as trying to look hand-made rather than being hand-made |

Both generations fail the same test for the same reason: neither is derived from the actual product. Swap the logo and headline on either and it still works for an unrelated company — that portability is the definition of slop, not the color or font in use.

## The full tell checklist

| Category | Tell |
| --- | --- |
| Fonts | Inter with no other rationale; the Space Grotesk / Instrument Serif / Geist rotation; a single italic serif word dropped into an otherwise sans-serif headline for "personality" |
| Color | Indigo→purple→pink gradient; cream/terracotta as its inverse; gradient used as a background wash rather than tied to any content; glow/box-shadow halos around cards or headlines; dark-mode-only with body text under 4.5:1 contrast |
| Layout | Centered hero + generic sans headline; badge pill floating above the H1; 3-up identical cards with icon-top and soft shadow; colored top/left stripe on cards; numbered 1-2-3 steps applied to something that isn't actually sequential; a stat-banner row of invented numbers; emoji standing in for icons, especially in a sidebar |
| Copy | Headlines that name no real capability ("Build faster. Ship smarter."); feature descriptions interchangeable across competitors; badges/tags added for texture ("Beta," "New," "Enterprise") with no actual status behind them; fabricated customer logos or testimonials |

Any one of these in isolation is a legitimate design choice. The tell is applying several together with no argument for why this subject, specifically, needed them.

## Operating procedure for agents

1. **Ground every choice in the subject before touching a palette.** Name the concrete product, its audience, and the one job the screen does. Derive color, type, and layout from that subject's own vocabulary (its units, its real data, its actual workflow) — never from "what SaaS pages look like."
2. **Ban the two default palettes outright unless the user asked for one of them.** No indigo→purple→pink gradient as a default hero treatment. No cream/ivory + terracotta + oversized italic serif as a default either. If the user's own words specify one of these looks, build it exactly — their instruction overrides this rule.
3. **Pick neutrals with a deliberate hue bias**, not a pure gray or a pure white/near-black chosen because it's safe. State in one line why this hue.
4. **Choose a type pairing you would not reuse on an unrelated project.** Reject Inter-by-default, and reject the Space-Grotesk/Instrument-Serif/Geist rotation as a substitute for a real decision.
5. **Use only real content.** No invented stats, no fabricated logos or testimonials, no placeholder numbers presented as facts. A number with no source becomes a labeled placeholder, not a plausible-looking fake.
6. **Give every recurring element (badge, tag, numbered step, colored card border) an actual informational job before adding it.** If removing it loses no information, cut it — it was decoration standing in for hierarchy.
7. **Cut ornament that doesn't represent live state.** A blinking or glowing status dot needs a real status behind it; a glow behind a headline needs no such test, so remove it.
8. **Run the swap test before shipping.** If the logo and headline can be swapped for an unrelated company's and the page still works unchanged, the design is slop regardless of which palette generation it belongs to — revise until it's specific to this subject.
9. **Take one real point of view per page**, and keep everything else around it quiet. Maximalist and minimalist directions both fail the same way when every element is trying equally hard.

## Sources

- [The generic style of AI web design](https://kylechayka.substack.com/p/the-generic-style-of-ai-web-design) — Kyle Chayka
- [AI Design Slop: 16 Patterns That Out Your App as Vibe-Coded](https://www.developersdigest.tech/blog/ai-design-slop-and-how-to-spot-it)
- [Why Every AI-Built Website Looks the Same (Blame Tailwind's Indigo-500)](https://dev.to/alanwest/why-every-ai-built-website-looks-the-same-blame-tailwinds-indigo-500-3h2p)
- [AI Slop Fonts and Gradients: The Tells That Give Away AI Design](https://www.925studios.co/blog/ai-slop-design-tells)
- [The Purple Gradient Problem: Why AI UI All Looks Alike](https://dev.to/james_anderson_h/the-purple-gradient-problem-why-ai-ui-all-looks-alike-and-how-to-fix-it-3j65)
- [Why Your AI Keeps Building the Same Purple Gradient Website](https://prg.sh/ramblings/Why-Your-AI-Keeps-Building-the-Same-Purple-Gradient-Website)
