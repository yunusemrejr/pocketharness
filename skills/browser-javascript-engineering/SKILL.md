---
name: browser-javascript-engineering
description: "Implement secure browser JavaScript with vanilla DOM, async state and selective Alpine or jQuery integration."
---

# Browser JavaScript Engineering

Use for browser application logic, secure DOM updates, progressive enhancement and legacy-library integration. Node runtime behavior belongs to node-runtime-engineering; frontend-js contains deeper event-loop/performance background.

## Working method

- Establish browser targets, module loading and existing state ownership.
- Validate data at network/storage boundaries; keep untrusted text out of HTML sinks.
- Own listeners, cancellation and stale results through mount/unmount or navigation.
- Test actual interaction, failure, keyboard and race behavior.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
