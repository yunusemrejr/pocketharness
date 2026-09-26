# Web Component Craft: patterns and examples

## Patterns that survive real content
A form field owns its label, hint, value and error association. Keep entered values after failure and move focus only when it helps recovery. A card groups related information; it is not automatically a clickable container with nested links. Tables represent comparisons, not arbitrary page layout. Preserve header associations, numeric alignment and meaningful empty states; virtualized grids need keyboard and assistive-technology testing.

```html
<label for="email">Email address</label>
<input id="email" name="email" type="email" autocomplete="email"
       aria-describedby="email-hint" required>
<p id="email-hint">We use this to send your receipt.</p>
```
When invalid, set aria-invalid and associate the specific error; do not claim that `type=email` validates account ownership.

## Dialog and navigation behavior
A modal needs an accessible name, sensible initial focus, focus containment, Escape policy and focus restoration to an existing trigger. Native dialog can provide useful mechanics; test supported browsers and nested overlays instead of assuming them. Popovers, menus, listboxes and comboboxes have different keyboard contracts. Use a proven primitive for complex behavior when the project allows it.

Tabs switch peer panels; links navigate locations. A disclosure uses a button with expanded state, not a clickable heading with no keyboard behavior. Breadcrumbs describe hierarchy, not browser history. Toasts supplement durable feedback; important failures need a persistent place and an actionable next step.

## Visual construction
Start with content hierarchy, consistent spacing and a restrained type scale. Use grid/flex with `minmax(0,1fr)` or `min-width:0` where intrinsic content can overflow. Long names, localization, 200% zoom and narrow displays are test cases, not decorative variants. Animation should communicate continuity, accept interruption and respect reduced motion. Avoid changing layout merely to make a demo screenshot look balanced.

Compare default, hover, focus-visible, disabled, loading, error and selected states. A disabled control may hide the reason an action is unavailable; sometimes an enabled action with explanatory validation is better.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.w3.org/WAI/ARIA/apg/patterns/
- https://developer.mozilla.org/en-US/docs/Web/HTML/Element/dialog
