---
name: libreoffice-automation
description: "Automate LibreOffice Writer, Calc, Impress and Draw on Linux with isolated headless conversion or UNO; use for office rendering, recalculation and ODF/OOXML interoperability."
---

# LibreOffice automation

Identify the installed LibreOffice version, supported filters and source document type. Use Writer for text documents, Calc for spreadsheets, Impress for presentations and Draw for supported drawings. Preserve original documents and select the final format from the user's workflow.

Read [headless jobs](references/headless-jobs.md) for process isolation and reliable conversion. Read [UNO and fidelity](references/uno-fidelity.md) when document APIs, recalculation or complex round trips are needed. Prefer a simple isolated conversion for exports; use UNO only when the task requires document-level control.

Give each independent batch job a unique writable user profile and output directory. Set a finite timeout and limit concurrency; office processes can retain locks or hang on malformed input. Keep macros disabled for untrusted documents and avoid automatic external-link updates. Do not stop the user's existing desktop office instance to make automation work.

Treat exit status as one signal. Verify a fresh output exists, opens, has expected document structure, and renders correctly. A successful conversion can still lose formulas, animations, fonts or layout. For conversions to PDF inspect pages; for recalculated spreadsheets compare key totals and formulas. Report fidelity in the actual renderer used. Keep temporary profiles scoped to the job, clean them after process termination, and deliver separate output files with any unsupported features identified.
