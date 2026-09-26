# UNO and fidelity

Use UNO when a simple conversion cannot express the operation: recalculate a Calc document, modify named ranges, inspect sheet state or set export options. Discover the installed Python/UNO bridge and service methods; a pip package named similarly is not proof it connects to LibreOffice. Use a local pipe or loopback-only listener, scoped to the automation job; never expose an unauthenticated office listener to the network.

Load documents hidden and use explicit macro and external-update policies. Save to a separate URL with the intended filter. Recalculate spreadsheet formulas in the office engine before saving and inspect representative results afterward. Set properties by their documented UNO types; a string that looks like a boolean is not a boolean. Close documents and dispose the owned office process in finally blocks. Keep bridge errors distinct from document errors.

OOXML and ODF are not feature-identical. Verify formulas, dynamic arrays, charts, comments, tracked changes, templates, fonts and presentation media according to the actual deliverable. Exporting and reimporting a PDF loses editability and is unsuitable for ordinary source-document changes. Draw can handle some PDF edits but does not restore original Word or slide structure.

Use application-specific authoring skills for spreadsheet, presentation or word-document content; this skill supplies process automation and cross-format checks. Consult the [LibreOffice SDK API](https://api.libreoffice.org/docs/idl/ref/index.html) for exact interfaces and properties rather than guessing method names. Keep output evidence concise: input/output mapping, engine version, completed semantic checks and actual visual inspection status.
