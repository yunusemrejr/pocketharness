---
name: data-lineage-validation
description: Trace numeric claims through source records, joins, transformations and denominators; use for hallucination-resistant datasets, reports and API reconciliation.
---

# Data Lineage Validation

Work backward from the requested claim to the exact records needed to establish it. Keep provenance in the existing notebook, query or report; do not invent a second data platform.

1. Inspect actual schema, timestamps, units, nulls and key uniqueness. Distinguish missing, zero, filtered and not collected.
2. Record source location/version, selection window and transformation code. If access is absent, mark the result unavailable; synthetic fixtures must be labeled synthetic.
3. Validate join cardinality before aggregation. Compare row counts, unmatched keys and totals before/after joins. Never repair duplicates with DISTINCT until their meaning is understood.
4. Recompute consequential values from source data. Check denominators and unit conversions separately; spot-check raw records including boundary and missing cases.
5. Tie each material reported claim to a query/output or cited source. A plausible model explanation is not evidence. When sources conflict, preserve both definitions until reconciled.

Example: joining orders to line items multiplies order totals. Aggregate at the intended grain before combining, then reconcile against source totals.

Deliver the reproducible calculation, provenance and unresolved data-quality limits. General literature search belongs to research; this skill verifies the path from records to claims.
