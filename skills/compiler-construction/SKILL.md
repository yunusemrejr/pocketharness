---
name: compiler-construction
description: Implement parsers, interpreters, compiler passes and semantics-preserving code transformations; use for DSLs, AST/IR rewrites and static analysis.
---

# Compiler Construction

Specify source-language semantics and the exact target subset. Reuse the project's parser and IR when possible; avoid regular-expression rewrites for syntax requiring structured context.

1. Separate lexing/parsing, name resolution, typing and evaluation/lowering concerns. Preserve source spans so diagnostics point to the original input.
2. Define precedence, associativity, scoping, binding and evaluation order. Include malformed input and recovery behavior, not only valid examples.
3. State each transformation's preconditions and preserved observations: returned values, effects, exceptions and termination as relevant. Preserve short-circuiting and evaluation counts.
4. Test transformations against an independent interpreter or reference implementation on bounded generated programs. Round-tripping alone can preserve the same bug in both directions.
5. For lower-level IR, inspect the installed target's overflow, aliasing, poison/undefined-value and calling-convention rules. Verify IR after each pass when tooling supports it.

Example: replacing `f() * 0` with zero can delete side effects or exceptions. Algebraic equality over numbers is not automatically program equivalence.

Deliver the grammar/semantics, pass invariants and differential tests. Consult the matching [LLVM language reference](https://llvm.org/docs/LangRef.html) only when targeting LLVM; do not impose its semantics on another IR.
