---
name: spreadsheet-authoring
description: "Create, edit and verify Excel XLSX/XLSM or LibreOffice Calc ODS workbooks, formulas, tables, charts and print layouts; use for spreadsheet deliverables."
---

# Spreadsheet authoring

Inspect sheets, used ranges, formulas, cached values, named ranges, external links, macros and charts before choosing an editor. Keep identifiers as text, distinguish blanks from zero, and establish dates, locale, currency and units from the source. Preserve the original workbook and user formulas.

For workbook creation and formula validation, read [formulas and data](references/formulas-data.md). For existing complex workbooks, formats and Linux interoperability, read [fidelity and delivery](references/fidelity-delivery.md). Load only the relevant reference. Use the available spreadsheet application or connector when fidelity requires native behavior; use installed Python libraries for supported transformations. Do not assume an installed library recalculates formulas.

Separate inputs, calculations and outputs through clear labels and cell styles. Derive totals from formulas rather than baking a second set of numbers into the workbook. Trace material figures to input ranges and verify representative edge cases, including empty input and boundary dates. Avoid turning untrusted strings into executable formulas.

Reopen the saved file, check formula errors and key totals after actual recalculation, and render affected sheets or print regions to inspect clipping, date formats and pagination. Report explicitly if recalculation or visual inspection could not run. Deliver the editable workbook, retaining requested file format and macros when supported, with a short note on assumptions and any unresolved compatibility differences.
