# UI Anti-pattern Review: patterns and examples

## Common failures and repairs
A neon pill marked “LIVE” is misleading when nothing is live. Remove it or connect it to a real status with a clear label and non-color cue. Monospace labels belong where they aid data/code reading, not automatically on every badge. Purple/blue gradients can suit a brand, but repeated decorative gradients without hierarchy create noise. Use restrained surfaces and one justified emphasis system when the content does not need spectacle.

Too many cards make related content look unrelated. Group by the user's task, use shared alignment and reserve borders/elevation for meaningful boundaries. Huge empty hero areas can hide the primary task. Show representative content density instead of evaluating only a perfectly short headline. Avoid replacing real product copy with made-up performance numbers or vague “intelligence” badges.

## Color and typography
Do not declare color pairs inherently forbidden: contrast depends on actual foreground/background values, size and use. Check text contrast and interactive/non-text indicators with a suitable tool. Never rely on red/green alone for status. Transparent layers and gradients require testing the worst background region. Use a limited type scale and deliberate weights; multiple display fonts or tiny all-caps labels can undermine reading. Verify fallback fonts, localization and zoom before declaring a pairing successful.

## Interaction and responsive behavior
A beautiful hover state is insufficient for keyboard/touch users. Check visible focus, labels, hit areas, disabled/loading states and recoverable errors. Avoid scroll hijacking, gratuitous motion and layout shifts; respect reduced motion while preserving essential information. Test narrow screens with long labels, tables and user content. A screenshot cannot prove a dialog traps/restores focus correctly or an animation handles interruption.

## Review format
For each issue state the observed element, user consequence and smallest coherent fix. Distinguish objective defects from aesthetic preference. Keep successful parts of the design; do not replace a mature product system with a fashionable kit. Compare before/after at matching viewport/content and verify the actual rendered result. If visual inspection is unavailable, report DOM/geometry findings as limited evidence rather than claiming pixel-level taste.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.w3.org/WAI/WCAG22/quickref/
- https://www.w3.org/WAI/ARIA/apg/
- https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion

## Contextual visual review

The following review cues are adapted from [Impeccable’s slop catalog](https://impeccable.style/slop/): inspect repeated gradients, blur, glow and decorative backgrounds; unnecessary nested cards and redundant borders/shadows; template hero/feature/metric compositions; redundant badges and labels; weak type hierarchy, tiny text and excessive uppercase; and motion that competes with the task. These are contextual cues, not prohibited styles. Respect the project's documented typography, colors and radii. Distinguish source checks, browser measurements and visual judgment; test contrast on the rendered surface. A restrained beige design can be as generic as a loud gradient design. Choose treatments because they support the content and interaction.

For each cue, record the element and consequence before changing it. For example, a border and shadow may usefully separate a floating panel, while the same treatment on every nested paragraph may fragment reading. Keep the successful parts. Compare matching viewport, content and interaction states after the repair.

## Component decisions and evidence

| Source cue | Default repair and verification |
|---|---|
| LIVE pill with blinking dot | Remove decorative status furniture. For required live data, show truthful status with a timestamp or stale/offline state; avoid recurring motion merely to imply activity. |
| Several primary font families or similar sans/display faces competing | Give body, heading and data text distinct roles. Reuse the project's type scale; check loaded/fallback fonts, long labels and zoom. Do not count language fallback stacks as competing faces. |
| Tiny uppercase text with wide tracking | Increase readability and reduce decorative metadata. Check the actual size and contrast; uppercase or monospace alone is not a defect. |
| Gradients, blur, glow and perpetual movement on the same screen | Keep effects that explain interaction or establish needed hierarchy. Test reduced motion and interruption; loading feedback can be functional. |
| Fixed-coordinate content or repeated nested cards | Use normal document flow and shared alignment where content must expand. Test narrow widths, localization, zoom and error messages; intentional overlays may stay positioned. |
| Clickable div/span, placeholder link or suppressed outline | Use native action/navigation semantics and a visible focus treatment. Exercise keyboard, pointer and failure states. External handlers and shared CSS need inspection before judging. |
| Pill clusters, hype badges, gradient text or glass panels | Pills mean selection, state or tags; badges mean taxonomy; type stays solid in the brand palette; glass floats above content. Keep at most the instances with a functional role. |
| Oversized display type or giant radii everywhere | Scale type and radius to the content's hierarchy. One expressive display line can stay; repeated 80px+ headlines and bento-grid radii go. |
| Emoji in chrome or fake terminal output | Drop the emoji unless the brand owns playful chrome; drop fake console lines unless the terminal is the product. Real icons, prose and UI states replace both. |
| Missing image alt, unnamed controls or skipped headings | Add concise alt (empty for decorative), names for every control, and a continuous h1→h2→h3 outline. These are defects, not taste; verify with keyboard and rendered inspection. |
| Autoplaying carousel, marquee or custom cursor | Make motion user-initiated with visible controls; restore the native cursor. Autoplay and cursor replacement need a central experiential reason to stay. |

No font-name or color-name blacklist can assess a composition. Explicit opaque CSS color pairs can be measured before rendering, but cascade, transparency, disabled/logo exceptions and font size still matter. The source checker reports only pairs below 3:1; normal text also needs evaluation against 4.5:1. Compare unrounded values to the applicable threshold. See [W3C contrast guidance](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html), [motion guidance](https://www.w3.org/WAI/WCAG22/Understanding/pause-stop-hide.html) and [button behavior](https://www.w3.org/WAI/ARIA/apg/patterns/button/).

Learned semantic-radar ranking estimates code reuse affinity, not visual taste. JSX contract drift stays visible and disables learned reordering within the affected candidate tier. A similar component may contain a changed label, tag, callback or ARIA attribute; inspect the difference rather than copying it or treating the score as a quality verdict.
