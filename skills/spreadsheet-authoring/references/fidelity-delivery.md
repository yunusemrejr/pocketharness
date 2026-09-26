# Fidelity and delivery

Choose the least disruptive supported edit. For a new XLSX, XlsxWriter is useful; for supported existing cell edits, openpyxl may suffice. Existing drawings, embedded objects, macros or uncommon extension parts warrant native application editing or a controlled package-level edit. openpyxl documents incomplete preservation of some objects; keep_vba preserves VBA content but does not execute it. [Preservation caveats](https://openpyxl.readthedocs.io/en/3.1/tutorial.html).

Keep XLSM macro-enabled and never silently save it as XLSX. Inventory package parts and external links before and after a library round trip. A ZIP that opens successfully does not prove chart, validation, protection or signature fidelity. Changing signed content may invalidate its signature; report that if applicable.

LibreOffice Calc is a Linux-native option for ODS and recalculation. Use the dedicated libreoffice-automation skill for isolated headless jobs. Verify conversion in the user's final application where available: formula support, named ranges, fonts, charts, print areas and page scaling can differ. Do not promote a Calc-rendered PDF as proof of identical Excel rendering.

For review render selected print areas; ensure columns are not cut, headers repeat where needed, and numbers do not show hash marks or unintended scientific notation. Maintain useful widths, frozen headers and filter controls without decorative clutter. Keep an editable source and a separate preview/export. Reconcile row counts, formula counts and material totals after saving; include changed cells or ranges in the delivery note when revising a financial or operational workbook.
