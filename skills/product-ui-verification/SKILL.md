---
name: product-ui-verification
description: Use when changing interfaces, native desktop apps/games, shared UI components or website presentation. Preserve product identity and verify real states instead of applying a generic aesthetic.
---

# Product-specific UI work

The user's design prompt overrides these defaults; a vague request is not permission to replace the product's identity.

- Inspect existing components, routes, styles, tokens, content and user tasks before choosing changes. Inventory usages, variants and states before editing shared components.
- Before a visual upgrade, retain a baseline capture at the normal viewport. Compare final work against it for composition, identity and illustration/3D craft. For motion or an animated character, also inspect representative frames and actual playback/interaction; a new static screenshot does not prove improvement. Preserve a better existing asset rather than replacing it with a simpler placeholder.
- Derive hierarchy, density, geometry, color and motion from this product; preserve justified design-system decisions. Avoid generic gradients, gratuitous glass, decorative “Live” badges, fabricated metrics and equal emphasis everywhere—without replacing those clichés by another fixed aesthetic.
- Make the smallest coherent change. Reuse established components and tokens; never invent a second button system, styling framework or configuration layer. Preserve behavior, copy and responsive layout unless the task requires changes.
- Check keyboard access, focus visibility, accessible names, contrast and loading/error/empty/disabled states. Verify narrow and wide layouts. State what was tested rather than claiming a comprehensive accessibility audit.
- Use `render_see` directly for supported browser inspection; it owns its installed renderer, so do not install Playwright merely to capture a page. It cannot verify interactions or GPU/WebGL/WebGPU output; use a separately available browser surface there, or report that limitation. On failure, diagnose the reported capability/runtime error before inventing a replacement script.
- Inspect semantics before pixels when that answers the question. `render_see {source,output:"text"}` provides bounded live-DOM evidence in an isolated, unauthenticated browser; `output:"both"` adds pixels for vision models. Set `colorScheme:"dark"` or `reducedMotion:"reduce"` for relevant media states. Neither performs login or interaction. DOM facts do not establish visual quality.
- For native desktop apps and games, launch through the normal entrypoint, capture the displayed window, and exercise real input, focus and exit. Offscreen modes and injected inputs are supplemental: they can pass while presentation or keyboard input is broken. Verify the capture belongs to the intended window. A failed capture is an unresolved observation, not proof of a compositor fault. Report an unobserved user path as unverified even when compilation and unit tests pass.
- When pixels are necessary and the active model lacks vision, use an explicitly vision-capable native subagent with only the image and a precise question, fresh context and no mutations. Verify capability first; return the artifact reference, observations and uncertainty. If unavailable, report visual verification as unavailable—never pretend to have seen an image.

Without vision, `render_see` text output includes CSS-pixel bounds and overflow measurements. Compare widths and investigate overflow and clipped controls, which may be intentional. These measurements do not establish contrast, occlusion, composition or aesthetic quality. Ask a vision child for specific hierarchy, alignment and state defects tied to the artifact, not a taste score.

Before the rendered pass, `artifact_check {operation:"ui",path:...}` can locate bounded source cues. Include truthful status, deliberate font roles and working controls in acceptance. Record viewport, state, revision, observed result and unavailable evidence so another worker can reuse the check. A later edit invalidates affected evidence.

Supply representative captures and behavior receipts to `quality_review` after checking the requested outcome. The interface reviewer must read current implementation source before inspecting up to three representative captures. Test logs cannot stand in for that source read. Resolve the standing anti-slop policy cues, including generic bulb branding, muddy orange/brass defaults, decorative label dots and boxed icons, before accepting the change; concrete user-requested branding/state exceptions need evidence. Also clear pill clusters, hype badges, gradient text, glass panels, oversized type, emoji chrome, fake terminals, missing alt, unnamed controls, skipped headings and autoplay before acceptance.
