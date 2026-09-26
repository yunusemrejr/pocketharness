---
name: browser-animation-engineering
description: Build precise browser animation timelines with JavaScript, WAAPI, requestAnimationFrame, interruptible transitions, deterministic capture, and measured frame budgets. Use for animation runtime behavior rather than general browser application code.
---

Choose one owner for each animated property and one time source for synchronized motion. CSS transitions handle simple state changes; WAAPI suits explicit playback and seeking; a requestAnimationFrame loop suits procedural state. Keep the project's chosen engine unless a demonstrated requirement justifies changing it.

Read [timelines and frame budgets](references/timelines-and-budgets.md) when implementing interruption, synchronization, FLIP, resize handling, high-DPI canvas, or capture. It includes velocity-continuity equations and worked budget calculations.

Separate timeline evaluation from DOM mutation: `stateAt(time)` should not depend on how many frames were displayed. For history-dependent simulation use a fixed-step state and documented seeking/replay behavior. Handle hidden-tab resumption and teardown explicitly, and preserve the requested final state when reduced motion disables movement.

Validate visible interruption, resizing, seeking and cleanup in a browser; pure timing arithmetic cannot establish layout or compositor correctness. Measure the actual scene at its intended refresh rate and density before calling an optimization successful.
