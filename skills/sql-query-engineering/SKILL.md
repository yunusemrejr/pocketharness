---
name: sql-query-engineering
description: "Design and verify SQL queries, transactions and migrations across database engines."
---

# SQL Query and Transaction Engineering

Use for relational logic and engine-specific behavior. The databases skill owns broader storage architecture; this guide owns executable SQL correctness.

## Working method

- Identify engine/version, schema, constraints and row cardinalities.
- Define grain, null behavior and tie-breaking before joining or aggregating.
- Parameterize values and allowlist dynamic identifiers.
- Check query plans, transaction behavior and migrations on representative data.

Read [patterns and examples](references/patterns.md) for the relevant implementation mode; do not load unrelated modes. Inspect the actual runtime, project conventions and constraints before choosing syntax, dependencies or deployment steps. User instructions take precedence; this skill adds no authority to change external systems.

## Evidence and completion

Use a small representative case and the relevant failure case to check the result. Report what was executed, what remains unverified, and any material compatibility assumption. Do not invent measured outcomes or treat reading this guide as verification.
