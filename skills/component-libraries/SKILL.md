---
name: component-libraries
description: Compare component architectures, form engines and data grids within an existing frontend stack. Use for deeper library adoption and customization tradeoffs.
---

# Component libraries (the choice + the adoption)

Start with `web-ui-stack-selection` for the compact selection/deployment workflow and `web-component-patterns` for actual component behavior. Package examples below are candidates, not current support or market-leadership claims: verify the installed version, official documentation and maintenance before choosing. Existing project conventions take precedence.

The question is never "which library is best" — it's **which architecture fits the product**, because the three architectures trade different things:

1. **Headless / unstyled** (Radix UI, Headless UI, react-aria, Floating UI): the library owns the *state machines* (the dialog's focus trap + `aria`, the combobox's typeahead + active-descendant, the tabs' arrow-key flow — the a11y plumbing that is *genuinely hard* to hand-roll), you own *every pixel*. The honest trade: you ship the design-system work (`design-systems`) — but that's the work you'd do anyway for a branded product.
2. **Styled systems** (MUI, Mantine, Ant Design, Chakra v3, PrimeReact, Bootstrap): full components + theming + dozens of edge cases solved (the datepicker's calendar math, the virtualized grid). The trade: *the look* — theming gets you "MUI-ish", "Ant-ish", "Material-ish", and past a customization point the theming-fights start (below).
3. **Copy-into-repo** (for example shadcn/ui): you own the copied component source and its updates. Distinguish this delivery model from packaged headless primitives such as Ark UI; verify actual dependencies and responsibility for accessibility fixes.

The meta-rule: **you will always own the a11y of the *content* (labels, flows, the error states) no matter what you adopt** — the library buys you the *interaction* machinery, not the correctness of your app. The adoption audit is the `ui-ux-principles` pass on the *result*, not the trust in the package.

## Candidate catalog (verify current compatibility)

### Headless (the behavior layer — pair with your tokens)

- **Radix UI** (React): the reference for the primitives (dialog, popover, combo box/`Select`, tabs, toast, toggle, the `asChild` slot pattern that makes them composable) — the state machines are the *industry-standard* implementations (most "shadcn-looks" are Radix under the skin).
- **Headless UI** (React): the Tailwind-native alternative; the slightly simpler API, the same class of state machines; the choice is mostly team familiarity.
- **react-aria** (React): the *accessibility-first* toolkit (the hooks: `useDialog`, `useComboBox`…) — the strongest a11y implementation of the big three, at the cost of the more-hands-on API (you compose the rendering, not grab a component).
- **Floating UI** (framework-agnostic): the positioning engine *under* everything (the popover/menu/dropdown placement with collision-flip, the auto-update on scroll/resize) — use it directly when your custom component needs placement you don't want to solve (it's the 20-line thing that's actually ~200 lines).
- The pairing: Radix/Headless for the *components*, Floating UI when you're outside their set, `focus-trap`-class for the non-Radix modals you hand-build.

### Styled systems (the batteries — the speed answer)

- **MUI** (Material UI): a styled system with theming and **DataGrid** (check feature and license boundaries for the chosen edition) + the Material look as a *fact of life* (fine for internal/B2B-tooling, the generic tell for a brand page).
- **Mantine**: React + hooks culture (the `useDisclosure`, the notifications, the date-input components are its stars); the cleanest "modern styled system", the theming works better than MUI's for non-Material designs.
- **Ant Design** (React): the **data-dense** standard (the table/form/breadcrumb ecosystem of the fintech/ops UIs); the Chinese-doc-first is a real friction for non-Chinese teams; the AntD-X line is the AI-chat UI add-on.
- **Chakra v3** (React): rebuilt (ark lineage, solid tokens story); the v2→v3 migration is the adoption footnote (the breaking changes were real).
- **PrimeReact / PrimeVue / PrimeNG**: the enterprise-grid tier (the *actual* data grids, the calendar, the chart) at the "enterprise component" look; the commercial tier for the premium components.
- **Bootstrap**: can fit an established product or a constrained delivery schedule. Check current supported releases, theme flexibility and migration costs; do not invent an upcoming major version or require a rewrite based on fashion.
- **Semantic UI / Foundation**: evaluate the actual package/version, security advisories and maintenance against project needs; do not infer abandonment from age alone.

### Per-framework

- **Vue**: **Vuetify** (the Material styled), **PrimeVue**, **Naive UI** (the TS-native, the clean), **HeadlessVue** (the Radix-lineage unstyled). Same architecture trade as React.
- **Svelte**: **Bits UI** (the Radix/Headless ports — the *unstyled-battery* answer that finally exists for Svelte), plus the mostly-hand-built culture (the Svelte idiom is the small custom component — `svelte`'s reactivity model makes the custom cheap, and that's *correct* for Svelte products, not a gap).
- **Cross-framework distribution** (the design-system-as-package, or the web-components-embedded-in-another-team's-app): **web components** (**Lit** for the build, **Stencil** for the framework-y build, or the hand-rolled `HTMLElement`) — the *interop* answer (works in React/Vue/Angular/vanilla), at the cost of the shadow-DOM styling boundaries + the event/a11y *bridging* pain (the framework wrappers — `@lit/react`-class — are the price). The enterprise-design-system story (the "we ship components to 5 product teams on 3 frameworks") is the one where web components *win*; the single-app story is "just use the app's framework".

### The supporting cast (per-function libraries — usually better than the system's built-in)

- **Forms**: **react-hook-form + Zod** (the *standard* pair — the uncontrolled performance + the schema validation + the `z.infer` types-through; the TanStack Form is the headless alternative, the Final Form the legacy), the **FormKit** (Vue's answer).
- **Data grids** (the ladder): **TanStack Table** (headless — you render, your design system wins; a candidate when custom rendering is appropriate) → **Glide Data Grid** (the virtualized *fast* grid, the spreadsheet feel, the custom-render cost) → **AG Grid** (the enterprise feature mountain — everything, the commercial license for the enterprise tier, the look is *AG*) → the styled-system's grid (MUI DataGrid / Ant Table) when the system is already there. The grid choice is a *web-patterns* decision first (does the data even need a grid, `databases`'s "the table is the UI" line).
- **Icons**: the framework-agnostic sets — **Lucide**, **Phosphor**, **Tabler**, **Material Symbols** (the per-set choice is a *design* decision, `svg-assessment`'s row-test applies to the *set* you pick: one family, the weight/cap consistent).
- **Charts**: `data-viz` owns the choice.

## The decision guide (the shape of the product decides)

- **Branded product (the look is the product)** → **copy-into-repo** (shadcn/ark) **or headless + your system**. The "we theming-fought MUI for 6 months and it still looks like MUI" is the *cost* of the styled-system architecture on a brand page — the architecture was wrong for the product, not the team.
- **Data-dense B2B / internal tool / ops console** → the **styled system** (Mantine, MUI, Ant, Prime) — the batteries (the grid, the date range, the a11y done) beat the bespoke at a fraction of the time, *and the MUI-look is a fine design decision at that role* (the "generic" tell only applies to the *brand* surface; the internal tool's generic is *correct* — the speed is the feature).
- **Marketing page** → **usually no component library at all** (the custom markup + `frontend-design`/`visual-composition`; the library weight + the component-granularity is the *generic marketing page* tell — the 12-section landing from a kit is recognized by the eye as "the kit").
- **Internal tool / fast prototype / the 2-week build** → **shadcn + Tailwind + TanStack pieces** (one option: copied source is editable, but you own integration and updates).
- **Multi-framework / the component-distribution case** → web components (`web-components`/Lit/Stencil), or the design-system package *per framework* (the port-story — usually the web-components, with the wrapper-pain accepted).
- **Existing jQuery/Bootstrap codebases**: preserve working conventions and improve the requested surface. Migrate when a concrete security, support or maintainability issue justifies it, with an incremental compatibility plan.

## The adoption doctrine (where the *good* libraries get ruined)

1. **The token bridge first**: the library's theming API maps to **your** tokens (`design-systems`'s one-source rule) — *never* the library's defaults-as-final (the "adopted the system and kept its palette/typography/spacing" is the *fastest route to "looks like a template"*). The library is the *mechanism*; the tokens are the *identity* (the shadcn's default shad-token set is a *starter*, the swap to your palette is step one, not step "someday").
2. **The theming fight has a shelf life**: theming a styled system is fine up to ~the point where you're overriding 40 CSS variables *and* patching components for the look — past that, **the custom (on the headless primitives) is cheaper than the theming debt** (the "we've customized 60% of the MUI theme and patched 8 components" is the tell — the architecture was the wrong fit; the honest migration is the headless + your layer).
3. **One family per app**: the **MUI + Mantine-in-one-app is the style chaos** (two focus rings, two z-indices, two datepickers, two "the" spacing scales) — one system (or the system + a thin custom layer); the *exceptions* are the *per-function* libraries (the TanStack Table doesn't "fight" the Mantine Button — different layers).
4. **The a11y is on *you* post-adoption**: the library gives the `aria` *scaffolding*; the *content* (the labels that describe the action, the flow that doesn't depend on the color-only state, the error that's *readable* not just visible) is yours — the `ui-ux-principles` audit runs on the *adopted* UI like any other UI (the "we used Radix so it's accessible" is the *most common* false confidence in the adoption story).
5. **The upgrade discipline differs by architecture**: the *copy-in* (shadcn) = the **upgrade is yours** (the diff-merge of the upstream components into your forked files, on your cadence — the *ownership is the feature* and the cost — budget the "upstream diff review" as a real task, or you drift); the *package* = the version-pin + the breaking-change budget (the major bumps have a *migration day*, the 5-second "npm i latest" is the incident).
6. **The lint gate per library**: the a11y lint (eslint-plugin-jsx-a11y, the framework's a11y rules) + the *library-specific* rules (the MUI/Chakra accessibility plugins, the Radix's `aria` correctness is built-in — the lint gate catches the *custom* drift around it).

## The kill list (the adoption anti-patterns)

- **The theming-fight** (above, #2) — the most expensive; recognize it by the patch-count, not the pain.
- **The framework mismatch**: the React library in the Vue app (or the vice-versa) — the *framework-agnostic* is Floating UI / the icon sets / the form-validation (Zod) — anything *component-tier* is framework-locked (`frontend-js`'s interop rule adapted to UI).
- **The mixed families** (two styled systems in one app — the 2-focus-ring bug) and **the mixed *architectures*** (half the UI on MUI, half on shadcn — the *worse* version of the mix: the look is the same "SaaS default" but the code has *two* ownership stories).
- **The hand-rolled state machine**: the custom combobox/dialog/tabs *when the headless primitive exists for your framework* — the a11y state machines are *exactly* the things to *never* hand-roll (the roving tabindex, the `aria-activedescendant`, the focus-return-on-close — 200 lines of edge cases with a decade of bug-fixes in the primitive); the custom is the *look*, the library is the *behavior*.
- **The generic-default look**: the un-themed system (the "it looks like every other MUI app" — the 2019-2023 SaaS tell) — the token bridge (above) is the fix, not the library swap (the "let's try Chakra" is the 6-week reroll of the same problem).
- **The bundle tax**: the 400KB for the *one* datepicker (the un-tree-shaken system import, the `import { Button } from 'mui'` full-bundle vs the named/granular import / the headless-alternative) — the `web-performance` bundle budget applies *per component library* (the grid is its *own* budget: the AG-Grid is a *page-level* dependency, the route-split is mandatory).
- **Maintenance risk**: inspect official releases, security handling, compatibility and unresolved issues. Low commit frequency alone does not establish abandonment; a stable library may change infrequently.
- **The copy-in without the *tokens*-swap**: the shadcn-adopted-but-shadcn-*tokened* — the "custom system" that's the shadcn look with new names (the row-test of the `frontend-design` catches it: the set isn't *yours*).

## The failure index (symptom → cause → fix)

- *"It looks like every other SaaS"* → the un/under-themed system, or the copy-in with the starter tokens → the token bridge (your palette/typography/radius into the library's token API) — the design work, not the code work; the *library swap* is the 6-week wrong fix.
- *"The library's combobox fights my select / the focus is weird"* → the two state machines in one control (the custom wrapper *and* the library's) → the one-owner rule: the headless component *owns* the state, your layer owns the pixels only (or the full-custom look on the *same* primitive — never a re-implementation around it).
- *"The theming is a bottomless pit"* → past the customization point (the patch-count tell) → the honest migration to headless + your layer (the `design-systems` build, *smaller* than the theming debt you're escaping).
- *"The bundle is 900KB and half is 'MUI'"* → the un-tree-shaken import / the full-grid for one table → the granular imports, the route-split (the grid page loads the grid), or the TanStack Table (headless, ~the table logic only, you render).
- *"The two libraries disagree on focus rings / z-index"* → the mixed families → the one-family rule; the custom layer over the *one* system.
- *"The modal steals focus and never gives it back"* → the hand-rolled modal (or the library without the focus management enabled) → the `<dialog>` (native) / Radix Dialog / the `focus-trap` — the *state machine* is the buy, not the write.
- *"The internal tool is gorgeous and the brand page looks MUI"* → the *flip* of the architecture mistake (the styled system on the *brand* surface, the bespoke on where the speed helps) → the brand page: copy-in/headless + your tokens; the internal tool: the system (the roles, not the reverse).
- *"We ported the design system to 3 frameworks"* → the per-framework port (the drift, the 3× maintenance) → the web-components extraction (Lit/Stencil) with the framework wrappers, the one-source-of-truth (the `design-systems` port line).
- *"The new features need a component the library doesn't have"* → the *extension* model is undefined → the copy-in (fork it, own it, keep the upstream-diff habit) — the styled-package's "missing component" is the moment its architecture shows its ceiling.
- *"The a11y audit failed on the 'accessible' library"* → the *content*-layer a11y (the labels, the flow, the color-only state) was never the library's job → the `ui-ux-principles` audit on the result (the fix is in *your* markup around the primitive, usually 10 elements, not the library).
- *Existing jQuery/Bootstrap app plus a new surface* → first check whether the existing stack can satisfy the request. If migration is justified, isolate style/state ownership and verify both surfaces; do not impose a rewrite or freeze working features automatically.
