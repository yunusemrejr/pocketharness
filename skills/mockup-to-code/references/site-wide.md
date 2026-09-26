# Site-wide generalization

One or two reference images rarely cover a site, but the request usually means all of it. The references define a system; every other page is an application of that system, not a new design.

## Extract and write down the system

From each reference: `image_analyze` → tokens, components, section patterns, voice (copy tone, density, imagery style). When two references disagree, prefer the value used more often or the one on the more important page, and note the conflict. Write a short design note: tokens, the component inventory (card, button tiers, input, nav, footer, section header, media block), the section rhythm (spacious hero, dense grid, statement band, dense call to action), imagery rules and do/don't lines. Build shared layout (header, footer, container) and components first; pages come last.

## Page archetypes

Classify every page and compose it from the inventory:

| Archetype | Compose from |
| --- | --- |
| Landing / home | Hero pattern, feature grid, proof (logos, quotes, numbers), call-to-action band |
| Listing / blog index / catalog | Section header, card grid (the reference card spec), filters as secondary buttons or pills, pagination |
| Detail / article | Narrow measure for body (60–75 characters), the type scale's headings, media blocks, related-items card row |
| Pricing | Card spec in columns with one highlighted tier using the primary color once, comparison table from type scale and borders |
| Form, contact, auth | Inputs derived from the system (surface or border color, radius, focus ring in the primary color, error color with text, labels above fields) |
| Dashboard / app | The palette on denser spacing (the lower half of the spacing scale), tables and charts using the same neutrals and one accent |
| Docs | Sidebar navigation, the body type level, code blocks on the surface color |
| 404, empty and error states | The hero pattern reduced, plain copy, one primary action |

## Inferring missing components

Derive rather than invent: an input takes the surface or border color, radius and focus ring of the buttons; a table takes the type scale, muted text for headers and borders from the border role; a modal is a card at the higher elevation with a scrim; a badge is a small pill in a tint of the primary. When a choice is not implied, take the plainest option within the system and record the assumption.

## Consistency checks

Identical header and footer everywhere; one container width; section padding from the two chosen values; one card spec; primary color used for actions only; the same icon family. Render one derived page at desktop and phone widths and show it before building the rest; the user's reaction to one page is cheaper than rework on ten.

## Restyling an existing site

1. Analyze the reference: tokens, components, type, imagery.
2. Audit the current site: `design_audit` for type sizes, spacing distribution, colors and contrast; read the stylesheet or theme config to find where tokens live.
3. Map old to new: each old color to a new role, the old type sizes to the new scale, radii, shadows, spacing.
4. Apply at the token layer first (CSS custom properties, Tailwind theme, design-token files), then adjust components whose structure differs (a flat button becoming a pill, cards gaining a border instead of a shadow).
5. Keep content, information architecture, accessibility and behavior unless asked otherwise.
6. Compare pages whose layout resembles the reference with `visual_diff`; judge the rest by the checklist above and `design_audit`.
