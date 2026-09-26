# TypeScript Contract Engineering: patterns and examples

## Types are not runtime validation
```typescript
type Outcome = { ok: true; value: number } | { ok: false; error: string };
function parseCount(value: unknown): Outcome {
  return typeof value === "number" && Number.isSafeInteger(value) && value >= 0
    ? { ok: true, value }
    : { ok: false, error: "Expected a nonnegative safe integer" };
}
```
A cast of JSON to an interface performs no checks. Validate at the trust boundary, then keep a typed internal representation. Structural typing does not distinguish dollars from meters; tagged/domain types help when their construction is controlled. Avoid non-null assertions that merely silence an unresolved lifecycle issue.

## Modules and libraries
Match module/moduleResolution to the actual bundler or Node mode. Path aliases accepted by the compiler may still fail at runtime. `import type` avoids runtime dependencies where appropriate; circular value imports can expose initialization bugs. Check declaration output, package exports and ESM/CJS consumers using the installed artifact. Do not assume a bundler test validates Node resolution. Keep public generic signatures understandable and avoid recursive type computation that slows editors without improving contracts.

## Application boundaries
Frontend DOM types, Node globals and TUI terminal APIs are different environments; separate configurations when ambient declarations hide mistakes. Model async states such as idle/loading/ready/failed rather than independent booleans that permit impossible combinations. Cancellation and stale-response guards remain runtime responsibilities. A typed event emitter still needs ordering, unsubscribe and lifetime rules.

Use strictness consistent with the repository and adopt tighter options deliberately. `noUncheckedIndexedAccess` and exact optional semantics can expose genuine assumptions but require careful migrations. Verify type-level expectations with deliberate negative compile fixtures when a public API depends on them. Verify actual errors, serialization and published exports with runtime tests; a successful `tsc` is not a working application. For generated types, keep schema generation owned by one source and validate compatibility rather than hand-editing outputs.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.typescriptlang.org/docs/
- https://www.typescriptlang.org/docs/handbook/modules/theory.html
