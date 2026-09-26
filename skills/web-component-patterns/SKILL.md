---
name: web-component-patterns
description: "Build accessible web UI components with concrete interaction, layout and state patterns."
---

# Web Component Craft

Use to implement a form, dialog, navigation, card, table or other browser UI element. This concerns component behavior and composition; library selection belongs to web-ui-stack-selection.

## Working method

- Name the user task and component states before styling.
- Prefer semantic HTML and the existing design system.
- Account for keyboard, touch, loading, empty, error and overflow states.
- Inspect at realistic widths with real content and test the complete interaction.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
