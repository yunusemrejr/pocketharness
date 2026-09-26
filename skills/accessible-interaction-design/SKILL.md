---
name: accessible-interaction-design
description: Design and verify keyboard, focus, assistive-technology semantics and error recovery for forms, dialogs and complex controls; complements visual UI design.
---

# Accessible Interaction Design

Start with the user task and state changes, not added ARIA attributes. Prefer native semantic controls and established project components when they meet the interaction need.

1. Describe loading, empty, validation-error, success and cancellation states relevant to the task. Keep entered data through recoverable failures.
2. Make labels and error associations programmatic. Separate focus from selection and give keyboard users a visible focus indication.
3. Test the entire task without a pointer. For a dialog verify entry focus, contained navigation while modal, Escape behavior when appropriate, and focus restoration on close.
4. Check zoom/reflow, reduced motion and accessible names. Automated accessibility checks catch some defects; they do not prove usable focus behavior or screen-reader output.
5. Verify the actual rendered interaction and report what was tested. Do not call the result compliant solely because a scanner found no violations.

Example: closing a deleted item's dialog cannot return focus to a removed button. Choose a meaningful remaining neighbor or section control.

Use the relevant [W3C interaction pattern](https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/) when implementing a custom widget; ARIA does not supply keyboard behavior. Keep visual composition in the existing design skills.
