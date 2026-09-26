# x86 Assembly and ABI Engineering: patterns and examples

## ABI before instructions
Linux System V AMD64 and Windows x64 differ in argument registers, preserved registers and stack rules. Kernel, interrupt and userspace conventions differ too. Confirm stack alignment at call sites and unwind metadata requirements. A userspace red zone is not safe in every environment. Intel and AT&T syntax reverse operand notation; assembler directives are not portable machine instructions.

```asm
; NASM syntax, System V AMD64 userspace: uint64_t add_u64(uint64_t a, uint64_t b)
global add_u64
section .text
add_u64:
    lea rax, [rdi + rsi]
    ret
```
This is unsigned modular addition, not overflow detection. It preserves callee-saved registers and does not touch memory. Signed source-language overflow rules remain relevant when replacing C/C++ code. A real object may need visibility and non-executable-stack metadata for its toolchain.

## SIMD and dispatch
CPUID support is not sufficient for every SIMD mode: the OS must enable required state, checked with the appropriate mechanism. Keep dispatch outside the hot loop and retain a compatible baseline. Handle zero size, short inputs, misalignment, overlap and vector tails without overreading unmapped memory. Wider vectors can change frequency, register pressure and numerical order; measure total work, not instruction count. Specify whether FMA rounding differences are accepted.

## Memory and verification
`volatile` in the host language is not a thread synchronization protocol. Compiler barriers, ISA fences and language-level atomics solve different ordering problems. Inline assembly constraints must describe all inputs, outputs and clobbers, including memory when appropriate. Prefer intrinsics when they express the requirement with less ABI risk.

Use disassembly, known answers and randomized differential tests. Include guard-page tail tests and calling-convention preservation checks. Timing instructions need serialization and accounting for migration/noise; prefer an established profiler or benchmark harness. Constant-time claims require an explicit leakage model and compiler/microarchitecture review; source-level branchlessness alone does not prove them.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- https://gitlab.com/x86-psABIs/x86-64-ABI
- https://www.nasm.us/doc/
