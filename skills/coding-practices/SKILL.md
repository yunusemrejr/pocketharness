---
name: coding-practices
description: Write and refactor maintainable code with clear naming, functions, types, errors and tests. Includes concrete examples and language idioms; follow existing project conventions.
---


# Coding Practices (with examples)

Treat the patterns below as defaults, not mandatory refactors. User scope and established language/project conventions win. Locate the existing owner of state and policy before editing; extend it rather than adding a parallel manager or source of truth. Share genuinely identical invariants, not code that merely looks similar. Use focused behavior checks and stop when the requested change is verified.

## Naming (the cheapest readability you get)

- **Intent-revealing:** `daysSinceLastBillable` > `d`; `retryCount` > `n`; `fetchInvoice` (verb) not `invoice` (nouns for functions are a lie).
- No type-in-name for values (`userList` → `users`; `flagBool` → `isArchived`); prefix/suffix for kind: `is/has/can` + adjective, `getX` (query), `setX/withX`, `onX` (handlers), `XError`.
- **Double negatives are a bug factory:** `!isNotDeleted` → `isActive`. A reader must invert twice; a future editor inverts once.
- **Consistency within the codebase beats convention on the internet:** the repo picks (snake/camel, `get`/no-`get`) and review enforces it. A mix is worse than either.
- A name is a comment: if the name needs a comment, the name is wrong. Rename the thing you're touching **first** (the rename commit is part of the feature, not an afterthought).

## Functions

- **One abstraction level:** a function does one kind of work ("compute the invoice total" or "persist it" — not both). If you need "and" in the english sentence, split.
- **0–3 arguments** (4+ → options object). **No boolean params** (a flag = two functions: `save()` / `saveAsDraft()`).
- Short is a guideline, not a law: 60 lines that read as one thing is fine; 12 lines with 4 branches is not. The test is: can you summarize it in one sentence without "and"?
- **No side effects hiding in reads:** `getUserCount` that also writes to a cache = a lie; explicit `touchUser()` beside it.
- Push I/O to the edges: `formatInvoice(user)` is pure (testable, no mocks); `processInvoiceFile()` owns the reading.

```ts
// before: 4 args, boolean, mixed concerns
function sendEmail(to, cc, subject, body, isUrgent) { ... if (isUrgent) markUrgent(to); ... }
// after
function sendEmail(msg: {to, cc, subject, body, urgency}) → Receipt { ... }
// markUrgent lives where urgency is a domain concept, not in email send
```

## Error handling (the discipline most code lacks)

- **Fail early, at the boundary:** validate at the edge (HTTP input, file, config, IPC) where the data enters; inside, assume invariants hold (re-validating `user.id` in function 7 is noise — or your 3rd function isn't in the right layer).
- **Exceptions/Results are for exceptional.** Control flow via errors = smell:

```python
# before (errors as flow):
try: row = db.get(order_id)
except NotFound:
    return None            # caller: if row is None: ...
# after (None/Optional is the contract; errors are truly exceptional):
row = db.get(order_id)     # → Order | None, documented
if row is None: return OrderDraft.new(order_id)
```

- **Never silence:** a `catch` must *do* something (retry with backoff, translate to a domain error with context, or propagate — never `catch: pass` and never "log and continue" on a failure that breaks an invariant).
- **Wrap with context** at each boundary:

```ts
try { return JSON.parse(raw); }
catch (e) { throw new InvoiceParseError(`invoice ${invoiceId} row ${i}: ${errMsg(e)}`, {cause: e}); }
```
  (the top of the stack tells the *where*; the chain tells the *why*.)
- **Type-based dispatch, not string matching:** `case e.code === 'ECONNABORTED'` / Rust `if let Error::Timeout` / Python `except PaymentProviderUnavailable` — matching `/failed/i` on messages is how your handler breaks when a vendor changes their wording.
- **Typed errors are part of the API** (a client switches on codes — see `api-design` Problem+json): define the error set for a module, document which are retryable.

## Immutability & state

- **Values are made, not mutated:** literals, `const`/`let` never `var`; Rust moves/borrows by default; Python: tuples/`dataclass(frozen=True)` at boundaries.
- **Mutation at explicit, narrow points:** mutate a *local copy*; the shared structure gets a replacement (CoW). Shared mutable state is where aliasing bugs and "it depends on ordering" live.

```js
// before (mutation into shared state):
cart.items.push(item); total += item.price;   // every holder sees mid-update
// after
cart = {...cart, items: [...cart.items, item], total: cart.total + item.price};
```

- **Single writer** for anything contended (a reducer/actor/model — one function that owns a state transition; everyone else sends an intent). Unidirectional data flow (event → pure transition → render) makes "where did this come from" a 10-second grep.
- Caches are LRU + capped, not "a Map" (see `frontend-js` for the leak patterns; same rule in every language).

## Types as contracts

- **`unknown` over `any`** at every external boundary; parse/validate (zod/Pydantic/serde) → the typed world. `any` in a signature is a TODO with a smile.
- **Every cast has a reason on the line** (`// cast: response guarantees X per spec §3`); no reason = delete or fix.
- **Exhaustiveness** (Rust `match`, TS `never` check, Python `assert False`) — the compiler/type-checker is your reviewer when a case is added.
- `@ts-ignore`/`# type: ignore` with a `# reason:` comment + issue ref, or not at all (unmarked suppression = the type system is off in that file).
- Discriminated unions over flags (see `frontend-js` TS section) — the type *is* the state machine.

## Branch structure

- **Guard clauses** (inverted, early) over nesting pyramids:

```ts
function publish(draft) {
  if (!draft.words.length) throw new Error("empty draft");
  if (draft.status === "published") throw new Conflict("already");
  const at = now();
  db.update(draft.id, {status: "published", at});
  return at;
}
```
- **Ladders of conditionals → a data table:**

```ts
// before: if role==='admin'... else if role==='editor'... else if ...
// after
const CAN = { admin: {publish:1, delete:1, refund:1}, editor: {publish:1}, viewer: {} } as const;
if (!CAN[role].publish) throw new Forbidden();
```
- For-each over the right primitive: `for…of` (not index gymnastics), array methods (map/filter/reduce) until a profile says otherwise; early `continue` over nested `if` inside loops.
- One `return` at the end? No — **many early returns are the point**; a single exit with 5 flags of state is obfuscated control flow.

## Comments (and the code they replace)

- **Why, not what** (what is in the code): why this hack, why this order, what the invariant is, what would break if you "fix" it:

```ts
// Order matters: index must exist before the backfill migration,
// otherwise the (tenant, created_at) query falls to a seq scan (incident 2026-04-12).
```
- **TODOs have an owner or a date, or they die** (`// TODO(2026-10): drop legacy path when v1 EOLs`); a TODO with no deadline is a permanent lie.
- **No commented-out code** (git is the archive); a block comment hides "is this still needed?" — if it's needed, keep it, if it's not, delete it.
- **Docstrings/exports:** what it takes, what it returns, what it can throw — for *public* things. Private helpers need a name, not a paragraph.
- Comments drift: if a comment and its code disagree, inspect requirements and tests to determine which is wrong; current execution is not proof of intended behavior.

## Testing (as practice, not artifact)

- **Test behavior through the public face:** the same output/state for the same input; not "the inner function was called with X" (refactor-proof).
- The three that earn their keep: **happy path · one real failure (typed, with the error contract) · one edge boundary** (0, 1, max-1, the locale, the 10k-item case). Not 14 tests of the happy path in different costumes.
- **Fakes over mocks for the outer world** (in-memory DB, a stubbed HTTP server, a fake clock); mocks only for narrow collaborator contracts. A fake that shares the real semantics catches more.
- **The mutation check:** after writing the test, *mutate the code* (flip a comparison, drop a clamp) — if no test went red, the test tests nothing. Red = keep the fix.
- **No test-only code paths** in production (a `forTesting()` flag = the test is testing a different program). If you need one, the seam is in the wrong place.
- Name tests as the assertion: `test("refund is blocked after payout settled", ...)` — the green list should read like the spec.

## Refactoring micro-moves (the small ones compound)

- **Extract** when you re-read a block twice (you needed help the first time, extract it now).
- **Inline** when a wrapper adds no meaning (a 1-line function called once = a hurdle, not an abstraction).
- **Rename on contact:** touching a module = a chance to fix the names in it (cheap now, expensive at PR 14 when 8 files use the bad name).
- **Delete first:** the PR with only deletions is the highest-value PR in the repo (lint-unused, knip, dead branches, the "for later" path that was never later).
- **Rule of three:** don't abstract cases 1 and 2 (they'll diverge); at the *third*, the pattern is real — abstract then (YAGNI at the refactoring scale, see `ponytail`).
- **Tell, don't ask:** `order.total()` not `total = order.items.reduce(...)` — the code moves to the type that owns the concept (the `Order` knows its own total; five call-sites don't).
- **Split by change-reason** (the SRP that's about *reasons to change*, not "one function per class"): pricing and tax live apart because they change on different schedules.

## Smells → fixes (the 10 that recur)

| Smell | Fix |
|---|---|
| God object (does 5 jobs) | split by change-reason; each piece owns its state |
| Shotgun refactor (one change = 9 files) | the concept is scattered; give it a home |
| Feature envy (function uses another's data) | move it to the other |
| Data clump (same 3 fields travel together) | make them a type |
| Primitive obsession (`"2026-09-06"`, cents-as-int) | a value type with validation (Date, Money, Email) |
| Magic number | named constant / config — with the *why* in the name |
| Boolean blindness (pairs of flags) | a small enum/state type |
| Duplicate logic in 3 places | extract (rule of three) — or deliberately keep 3 variants with a comment *why they differ* |
| `utils.ts` with 40 functions | the functions go home, to the domain they serve |
| "Utility function" with side effects | split read and write (the name is a contract) |

## Language idioms (short, high-frequency)

- **TS/JS:** `strict` + `noUncheckedIndexedAccess`; ESM; discriminated unions; `satisfies` for config; `unknown` at boundaries, parse with zod/valibot; no `var`, no implicit globals; `for…of`; `structuredClone`.
- **Python:** type hints on all new code + `mypy --strict` where green stays green; `dataclass`/`pydantic` for data; `pathlib` over `os.path` strings; `with` for every resource (files, locks, HTTP); f-strings; `enumerate`/`zip`; exceptions specific (`except PaymentError` never bare); `__all__` on public modules; no `eval`/`exec` ever.
- **Rust:** `?` propagation (no 8-level `match` ladders in business logic — newtype the error); `unwrap/expect` only at true boundaries with a message; small `unsafe` blocks with a `// SAFETY:` comment that states the invariant; `clippy` at pedantic-warn; iterators over manual index loops; `#[must_use]` on fallible reads.

## Review checklist (≤ 2 minutes, catches 80%)

Correct behavior on the change · error paths handled (or the happy-path-only is deliberate) · naming consistent with the module · a test that fails if the behavior changes · no new duplication (3rd occurrence = extract) · no new any/ignore/suppression · the diff deletes as much as it adds (ideal) · performance claim backed by a profile, not a guess.

## Detailed coverage

Concrete best-coding-practices with before/after examples — naming, function design, error handling, immutability, types as contracts, branch structure, comments, tests as a practice, refactoring micro-moves, code smells → fixes, and per-language idioms (TS/JS, Python, Rust). Use when writing or reviewing code, cleaning up a module, or establishing team standards.

## Recovering an exact-text edit
Read the current target region before constructing `oldText`; use verbatim text, not remembered or reconstructed formatting. A search result or another agent's summary may locate the code but does not replace the read required by the edit tool. After a rejected batch, check whether the tool reports no changes or partial changes. Retry the full corrected batch only when nothing applied; otherwise retry only failed edits. Re-read after formatters or concurrent changes, preserve unrelated work, and never bypass a failed exact match with a broad whole-file overwrite. Compaction does not make remembered source authoritative.
