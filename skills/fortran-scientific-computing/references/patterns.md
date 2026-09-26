# Fortran Scientific Computing: patterns and examples

## Explicit contracts
```fortran
module vector_ops
  use, intrinsic :: iso_fortran_env, only: real64
  implicit none
contains
  function squared_norm(x) result(v)
    real(real64), intent(in) :: x(:)
    real(real64) :: v
    v = sum(x*x)
  end function
end module
```
This simple norm can overflow or underflow for extreme magnitudes. A scaled BLAS norm is preferable when the domain requires wide dynamic range. `real64` selects a storage kind; do not infer every IEEE behavior without checking the compiler/platform. Use `iso_c_binding` kinds at C boundaries and verify value versus reference conventions.

## Arrays, parallelism and legacy code
Fortran column-major storage makes the first index contiguous. An assumed-shape argument needs an explicit interface. Noncontiguous slices can create temporaries; declaring contiguity without meeting its contract is not an optimization. Check array lower bounds, allocation status, sizes and ownership. Avoid relying on implicit SAVE or compiler initialization quirks. Replace COMMON blocks incrementally with tests around actual scientific outputs.

`do concurrent` asserts independence; it does not guarantee parallel speedup and must not conceal reductions or shared state. OpenMP reductions reorder arithmetic. Coarray execution and BLAS threading need supported runtimes and a deliberate process/thread plan to avoid oversubscription. Check BLAS LP64 versus ILP64 integer interfaces before linking large problems.

## Numerical checks
Use manufactured solutions, conserved quantities, convergence under refined step/grid and residuals appropriate to the equation. Compare against an independently implemented small reference; two builds of the same bug are not independent evidence. Enable bounds, uninitialized-use and floating-point diagnostics where supported in debug configurations. Optimization flags such as fast-math can change NaNs, signed zero and reassociation; retain the accuracy contract. Benchmark representative shapes and memory traffic, including I/O, rather than only an isolated loop. Preserve input units, coordinate conventions and provenance in restart/output files.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://fortran-lang.org/learn/
- https://gcc.gnu.org/onlinedocs/gfortran/
- https://www.netlib.org/blas/
