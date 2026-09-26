---
name: web-patterns
description: Choose product UI patterns for navigation, onboarding, forms, search, tables, pricing and settings. Use when deciding interaction structure rather than visual styling.
---


# Web UI patterns (the catalog)

The job of a pattern: the user has already seen this shape a hundred times. A *correct* pattern is free comprehension; a *misapplied* one is a tax on every interaction. Each section below is the shape, the when, and the kill-list. Element-level quality (spacing, type, hover states) is `ui-ux-principles`/`design-systems`; the words are `copywriting`.

## Navigation
- **Top bar (the web default)**: logo left, primary nav left-of-center, search + primary action + account right. Keep **≤ 5 top-level items** — more means a mega menu or an IA redesign, not item #6. The nav is furniture: quiet contrast, never competing with content.
- **Mega menu**: only past ~15 total items. Grouped columns of one-line items + one-clause descriptions. Open must be hover-intent (~300ms) *and* keyboard-operable (focus opens, arrows move, Esc closes) — the mouse-only ghost menu is the a11y violation.
- **Sidebar (app IA)**: max 3 levels, grouped with labels, collapsible to icons with a visible handle. The **active state must pop** (accent bar or bg step + full-weight label — bold alone doesn't). A persistent bottom CTA fits creation-heavy apps only.
- **Tabs vs subnav**: tabs are the *views of the current object* (≤5 peer states); different objects or depths belong in subnav/breadcrumbs. Tabs-as-pages for separate app sections is the IA misplaced in the header.
- **Breadcrumbs**: for depth > 3. Crumbs are *links* to ancestors; a non-navigating crumb is a label, not a crumb. Mobile: title + back arrow replaces them.
- **Bottom nav (mobile shell)**: 3–5 items, home first; the center slot is for the create action, not a sixth destination.
- **⌘K command palette**: an *overlay* (the page stays under it, Esc returns), unifying object search + actions ("Go to Settings", "Switch team"), recent-first ordering, arrow+enter, and a **create "no result X"** exit. The palette adds speed; the nav still owns discovery.
- **Kills**: nav that *is* the hero on a 3-item site; "Dashboard" and "Home" pointing at the same place; hover-only menus; two information architectures in one bar.

## Forms (where most UI fails)
- **Labels, always, persistent** (above or beside). The placeholder is *not* a label — it vanishes on input. Floating labels only via a library that nails a11y (Radix/Headless do; hand-rolled usually don't). Helper text states the *rule* ("3–50 characters"), never the name.
- **One screen vs wizard**: ≤5 fields or one effort-level → one screen. Above that → wizard: one job per step, *named* steps ("Payment", not "Step 2"), visible progress, and **back must never lose data** (values live in state; the 10-minute-entry-loss-is-one-back-button click is the #1 form incident).
- **Validation timing**: on **blur**, not per keystroke (live checks only for pass-only feedback: username availability, typeahead). Errors sit *next to the field*, never in a transient toast. **On submit: re-check everything and move focus to the first failed field** — the focus move *is* the affordance.
- **Submit states**: Save → Saving… (spinner *in* the button, disabled — double-submit is the classic data-corruption bug; disable on click, idempotency key on the server — `api-design`) → success state or *inline* failure ("couldn't save — check the highlighted fields").
- **Autosave** (where content is the user's: docs, drafts, long forms): "Saved 12s ago" indicator + ⌘S that *works* (wire it to the same save — a dead ⌘S is a broken promise) + visible conflict handling (last-write-wins *with a notice*; silent overwrite is the incident).
- **File upload**: drag *and* browse (drag is a bonus, browse is the path); type + size limits **before the attempt** ("30s upload fails at 99%" is the anti-pattern); per-file progress; **per-file retry** while the queue continues; queue states visible ("3 uploading, 1 failed").
- **Dates/numbers/money**: native `type=date` on mobile; range picker for date spans; money fields edit as plain numbers (**format on blur, never while typing** — the fighting-cursor bug); currency in the label or a right-aligned suffix.
- **Combobox (search-in-list)**: filter-as-you-type, debounced 150–300ms with cancelled requests (AbortController — `frontend-js`); no-results shows **"create 'X'"** — the add-on-type combobox replaces a separate "add" button entirely; multi-select → chips + select-all + count + the bulk bar appears at ≥1 selection. >50 options means a *search list*, not an alphabetized dropdown.
- **Passwords**: show/hide toggle; strength meter that measures real things (length + breach check — the "1 symbol + 1 number" checklist teaches "Passw0rd!" and measures nothing; `web-security`); **autofill attributes are mandatory** (`autocomplete="username"/"new-password"` — half of "my password manager broke" is missing attributes).

## Data tables
- **Columns**: name/subject + status first; timestamps last (always sortable). Sort arrows on active headers only. Per-column filter row under headers (text + dropdown) is the power pattern; a global bar is the 80% pattern — never both doing the same job.
- **Pagination vs virtualization**: page (20/50/100) + **"1–20 of 1,204"** — silent pagination is a bug. >~100 visible rows → virtualize (`frontend-js`). Infinite scroll is wrong in tables (it destroys position reference); paging is the position.
- **Selection & bulk**: header select-all with a "select all 1,204 (filtered)" escalation — always the *filtered set*, named as such. Bulk bar appears on selection ("3 selected → Tag · Move · Delete"). Bulk destructive confirms **with the count**.
- **Row actions**: ≤3 inline, the important ones as text ("Pay"), the rest in a row-end ⋯ (destructive last). Pick *one* meaning for row-click per table (open, or select) and keep it consistent.
- **Row detail → drawer, not page**: 480–560px right, esc closes the drawer only, table frozen beneath. Pages are for deep objects with their own navigation.
- **The two empty states** (never conflated): **"no items yet"** → create CTA; **"no matches for filters"** → list the applied filters as removable chips + reset. Conflating them reads as *data loss* — the panic bug.
- **Loading**: skeleton rows **matching the column layout** (never a spinner over the table area); "load more" button below sectioned tables; export CTA with **scope named** in the confirm ("the 1,204 filtered rows" vs "all 84,000") — async export + notification for >10k rows.

## Detail objects
- **Header**: title + **status badge** (state is the first thing read after the title) + contextual primary action ("Pay" when unpaid) + ⋯ (destructive last).
- **2-column**: main 60–68% + **sticky** meta rail 32–40% (facts, assignee, references — sticky is what makes the layout work; both-scrolling loses the facts after 300px).
- **Long content**: H2 sections; accordions for secondary ones (first 2 open, "expand all" as a text link). Timeline: reverse-chronological "Maya moved this to In review · 2h ago", field changes shown as **diffs** ("Status: Review → Approved"), "show earlier" paging — not infinite scroll on an object page.
- **Comments**: *anchored* (margin icon on the paragraph — documents, design reviews) vs *threaded* (one stream + @mentions — records). Indent max one level. Resolve collapses **in place**; resolved threads move to the bottom, never deleted (the history is the object's truth).
- **Version history**: who/when/what-clause list; **compare against current** as the default; restore is **non-destructive** (creates v13 from v12, never deletes the old one).

## Search results (the page)
- Count first ("42 results for 'invoice'"); facets left with counts ("Open (18)"); **applied filters as removable chips** under the bar; sort as a labeled control, relevance default; 20/page.
- Result item: title with **match highlighted** (tint + underline or bold — the raw yellow-`<mark>` fails contrast for some), context snippet around the match, type + date meta.
- **No-results is the most-designed state in search**: "No matches for 'xyz'" + try-these suggestions + "remove a filter" naming the filters + the create path where creatable ("No reports for Q3 — create it"). Live search: 300ms debounce, keep old results until new ones land (flash-to-empty is the jank).

## Destructive actions (the tiers — words in `copywriting`, this is the shape)
- **Reversible → no confirm, UNDO**: delete → gone + toast "Deleted — Undo" (8–10s window; undo restores *exactly* — position included). Trash with a 30-day window is the stronger version; the *empty-trash* is the real destructive and is named as such.
- **High-cost, mostly reversible → named confirm**: modal for settings-page nukes; **inline expansion** for in-list bulk ("Delete 14 exports? Can't be undone. [Cancel][Delete]") — inline beats modal inside a list. Name the object **with its count**; the destructive button says the verb, never "OK".
- **The nuke (org/workspace/account) → type-to-confirm** the exact name, scope *listed* ("142 projects, 8 members, all history"), the red at the end, "This can't be undone." as the last word; org-level adds the confirm-email with a 24h window (`web-security`). Export-first: the delete confirm *includes* the "download your data" path.
- **Kills**: confirms on the *reversible* (confirm-fatigue trains reflex-yes, which is how the nuclear click happens — save the friction for the nuclear); "OK" as the destructive label; gray delete next to primary; undo that half-restores; triple-locks (type + checkbox + code) on mid-tier deletes.

## Auth & consent shapes (UI only — the security is `web-security`)
- **Login**: SSO/social buttons **primary**, email under an "or"; B2B → SSO-first, consumer → social-first. Password field: show/hide + autofill names. "Forgot password" as a *text link* (it's secondary — never a button). One hero method, then the rest.
- **Sign-up**: ≤3 fields; **progressive profiling** — team-fields at invite, company at billing, never "tell us everything now". One value line above the form. Email-verify is its own *screen*: masked address ("m***@x.com"), resend with 60s countdown, "wrong email?" correction.
- **Magic link**: "we sent a link to m***@x.com" + not-you link; resend behind a countdown (never free-click — the spam vector); the link lands on the *named* action.
- **2FA**: QR **plus visible manual entry** (the scan-fail path is ~30% and the a11y floor — show the secret as copyable text, it's a tension to resolve in favor of UX: the screenshot *is* the safe backup flow); backup codes shown once with a "download .txt" CTA; passkey gets the platform button.
- **Logout/session**: "log out of all devices" = named destructive-tier-2 (it names what it kills); account menu shows email + plan + switch-account for multi-profile users.
- **Cookie consent**: bottom **bar**, not modal (modal = the aggressive shape); "Only necessary" and "Accept all" at **honest weights** (the tiny-reject dark pattern is a legal risk and an ethics line); consent is a *setting* — re-openable from a footer link; pre-checked boxes are banned in the EU.

## Settings
- **IA**: left groups (Account · Preferences · Workspace · Notifications · Billing · Data · Advanced · **Danger Zone last**), ≤8 groups; past ~50 settings add **settings search** (⌘K inside settings, over names + one-line descriptions).
- **Save model**: autosave-per-change for toggles (the "Saved" toast per tick *is* the confirmation, visible state on the control); named confirmation for critical ones ("2FA is now on"); a real Save button for heavy sections (billing). **Silent save on a critical setting is the "did it take?" bug.**
- **Controls by semantic**: toggle = binary preference (label states the effect: "Weekly digest — Mondays 8am"); 3–5 meaningful choices = radio list (the 3-option dropdown is the hidden-choice bug); slider = continuous with a **live value readout**; nothing is a slider with two positions.
- **Danger zone**: last section, red-tinted border (buttons not red yet); per action: name + scope + irreversibility stated **per action** ("This can't be undone."), not per section; account-delete at the very bottom with tier-3 + confirm-email + 30-day window + the export step inside.

## Feeds, notifications, toasts (the transient layer)
- **Feed**: reverse-chronological, sticky date-group headers ("Today"), pull/manual refresh, **100-item cap → "view all"** on pages (the app feed may go infinite; the page feed must cap), and a **"N new" bar** instead of silent top-inserts (silent inserts move content under the eye).
- **Toast**: bottom (bottom-right web / bottom-center mobile — top is the browser's territory). 3.5s non-actionable, **8–10s with Undo** (the undo window *is* the toast's life). Stack cap 3. Never steal focus — `role="status"` polite; assertive only for errors. A toast per field is a design hiding an over-chatty backend.
- **Badge**: **99+ cap** (4,812 is anxiety UI); zero is *removed* (the empty badge is a bug); one home per badge (object icon *or* nav item, not both); web: document-title count "Inbox (3)" + PWA badge from the same state.
- **Notification center**: icon + text + time + **per-item action**; read/unread state + filter by type + "mark all read" as a text link top-right; the empty state is *relief* ("Nothing needs you"). The digest email: opt-in, **important-only by default** — the everything-digest is the unsubscribe factory.

## Dashboards (layout — the charts are `data-viz`)
- **The KPI row**: 3–5 numbers, one row, each = the number (disproportionately large) + label + delta ("▲ 12% vs last week", direction + period) + optional 14-day sparkline. Then a 2-col chart band — **the dashboard answers one question**; the 12-chart wallpaper is the anti-dashboard. Table of the detail at the bottom.
- **Drill-down spine**: KPI → filtered list → detail row; each level *narrows*. Break the spine and it's a collage, not a dashboard.
- **Date range**: global sticky control ("Last 30 days · compare: previous 30d") with **compare on by default** (period-over-period is the first feature, not the fifth); granularity toggle only where the range allows it.
- **Share/schedule**: share = read-only signed link (the CSP `frame-src` must match the BI embed origin — `web-security`); schedule = the named recurring report ("every Monday 8am to #ops") — subscriptions are the retention feature.
- **Empty dashboard**: *is* the onboarding screen — the 3-step get-started **as the empty state**; sample data only as a **labeled, removable** mode; never fake charts (the "is this real?" kill).
- **Loading**: shell first, KPI skeletons (number-shaped, not bar-shaped), then charts; **stale-while-revalidate** — previous data + "updated 2h ago · refreshing" beats the 5s blank (the blank is the "is it dead?" moment).

## The 4-state doctrine (apply to every section)
- **Empty** — three kinds, three stories: *first-time* (create CTA, illustration optional — hand-drawn two-color or nothing; stock 3D people are the "AI-made" tell), *filtered* (removable filter chips + reset), *consequential* ("your trial ended" — what happened + what next + a human path; the "talk to us" is a CTA, not a footer link).
- **Loading** — the time budget: <100ms nothing; 100ms–1s **skeleton in the content's shape** (numbers, rows, avatars — never a spinner over a layout); 1–10s named progress with a *real* cancel; >10s it's a **job** ("we'll tell you when" + leave — durable completion, survives the tab close). Optimistic UI: local echo now, **rollback announced** on failure — the silent revert is "where did my edit go?".
- **Error** — inline (field) / **per-widget** (one failed chart doesn't kill the dashboard — fault isolation is a *design* rule) / page (404: search + home + closest-match, voice per brand; 500: "something broke on our side" + **request-id** + retry respecting `Retry-After`). **The 500-with-request-id is the highest-ROI error design in the web** — it turns "it's broken" into a 10-minute diagnosis. Auto-retry only idempotent reads; the POST stays a user-decided button (`api-design`).
- **Full** — the least-designed state; design it anyway: density by context (`desktop-ui`), the toolbar states, the overflow.

## E-commerce (the revenue path)
- **PDP**: gallery = consistent aspect (all same crop), swipe/tap between, zoom in-place (not a page), "2/8" counter; variants as chips with selected state pop, **combo-aware stock** (red out → XL disabled); price with honest "was" (the fake anchor is the FTC line — `copywriting`); **returns + shipping estimate above the fold** (estimate by zip — trust before the buy decision); add-to-cart is the largest element on the fold; description/details/returns/shipping as sections; reviews: histogram + count + verified badge + photo grid; sold-out → "notify me" (email, verified, the 60s-resend rule).
- **Cart**: **the drawer, not the page** (400–480px, context preserved beneath; the page is the deep review). Row = image + name + variant + line price + qty stepper + remove (remove = tier-1: undo toast). **Free-shipping progress bar is the conversion feature** (only with a real threshold — the fake countdown is the dark pattern). Promo input with honest applied/invalid states. Subtotal is *not* the total: "shipping + tax at checkout", stated, never discovered.
- **Checkout**: guest-first (account creation *after* purchase or a one-line "create account" checkbox the user ticks); address autocomplete (the 5-field geography is the drop-off); shipping options as radio with **price + duration** both visible; payment field with the `autocomplete` names + card-brand detection; **no surprise line items** — the fee that appears at the last step kills the order, fees go in the cart drawer early; the order-confirmation screen shows what happens next (delivery window, tracking link, "what to do if it's wrong" — the support path *is* the conversion recovery).
- **Kills**: the forced-account wall before cart; "as low as $49" pricing gymnastics; auto-adding products to the cart; the "only 2 left" that's never true.

## The 5-minute pattern audit (any screen)
1. **Nav**: ≤5 top items? active state pops? ⌘K reachable? one IA only?
2. **Forms**: labels persistent? validate on blur + focus-first-error? submit states? autosave where content is theirs? autofill names present?
3. **Tables**: count visible? the *two* empties distinct? bulk bar? drawer for detail?
4. **Search**: applied filters removable? no-results has a real exit?
5. **Destruction**: tiered? reversible gets undo (no confirm)? nuclear gets type-to-confirm + listed scope?
6. **Settings**: save model named? controls match semantics? danger zone last, per-action irreversibility?
7. **Transient**: toasts bottom, ≤3, undo 8–10s? badges capped, zero-hidden? "N new" instead of silent inserts?
8. **States**: all 4 designed in every section? request-id on the 500? per-widget isolation?
9. **Dashboard**: one question? compare default-on? drill-down spine?
10. **Revenue path**: account wall absent? fees early? confirmation screen has "what's next"?

## Detailed coverage

The canonical web UI patterns and when to use each — navigation (top bar, mega menu, sidebar, tabs vs subnav, breadcrumbs, command-palette search), landing/pricing page shape, onboarding (progressive profiling, connect-first, sample data, tours), forms (labels, validation timing, submit states, autosave, uploads, comboboxes), data tables (sort, filter, pagination vs virtualize, bulk bars, row drawers), detail pages (2-col, timelines, comments, version history), search results (facets, no-results, highlighting), destructive-action confirmation tiers, auth & consent shapes, settings (save model, danger zone), feeds & notifications (toast vs badge vs in-app), dashboards (layout, KPI row, date range), the 4-state doctrine (empty/loading/error/full), e-commerce (PDP, cart drawer, checkout, reviews). Use when choosing the shape of any UI section — "what is the right pattern for this job" — or when reviewing a screen for pattern misuse.
