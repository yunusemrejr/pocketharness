# Layout and authoring

Start from the supplied theme and slide dimensions. Reuse master/layout relationships, then add a small grid and consistent text hierarchy. In python-pptx, work through the documented object model and explicit Inches/Pt conversion; its underlying geometry uses EMUs. Text frame paragraph and run formatting are separate. Assigning entire text replaces runs and can discard local formatting; edit narrowly when preserving existing typography. [python-pptx quick start](https://python-pptx.readthedocs.io/en/latest/user/quickstart.html).

Use actual rendered text dimensions for dense content. Automatic text fitting can shrink the whole frame; impose a readable minimum and split the slide or rewrite before accepting tiny text. Font availability influences fitting. Do not rely on a template's placeholder index meaning the same thing in another layout.

Match charts to comparisons: bars for categories, lines for ordered time, scatter for paired continuous measures. Record units, denominator and source near the chart. Keep chart data editable when supported; verify displayed values against the source table. Distinguish zero from missing observations. Use a stable palette across the deck; all series and legend colors must agree.

For diagrams, connect the actual objects and keep arrow direction meaningful. A background screenshot is unsuitable when the user needs to edit labels or nodes. For photos, calculate crop from image and box aspect ratios, preserve focal content and retain enough resolution for the final canvas. Keep source/license notes when supplied. Build notes separately from slide copy; do not turn notes into hidden off-slide text.
