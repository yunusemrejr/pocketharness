# C++ Performance Engineering: patterns and examples

## Lifetime and API design
Prefer values, standard containers and unique ownership. Use shared_ptr only for actual shared lifetime; cycles and atomic reference traffic are costs. string_view/span are borrowed views and can dangle after owner destruction or vector reallocation. Return references only when the lifetime contract is enforceable. A noexcept promise must match implementation and member behavior; an exception escaping it terminates.

Separate a stable C-compatible plugin/FFI boundary from compiler-specific C++ types when binaries must interoperate. Do not pass STL allocations between incompatible runtimes. Hide architecture-specific kernels behind a tested dispatch layer rather than scattering intrinsics throughout domain code.

## Rendering and simulation example
A particle step should have one owner of time and state:
```
v_next = v + dt * acceleration(x)
x_next = x + dt * v_next
```
This symplectic-Euler order differs from explicit Euler. Neither is exact; compare timestep refinement, energy drift and known solutions. Keep simulation ticks separate from render interpolation. For high particle counts, compare structure-of-arrays against array-of-structures using the actual accessed fields; include upload/copy cost and GPU synchronization.

## Numerical and ML kernels
For blocked matrix multiplication, define strides, leading dimensions, aliasing and accumulation dtype. Compute a scalar/BLAS reference on rectangular, degenerate and tail sizes before vectorizing. Bound tile working sets by measured cache behavior, not a copied constant. Reordered reductions and parallel sums can change floating-point answers. Statistics need stable algorithms: Welford-style variance avoids subtracting two large nearly equal quantities, but merging and weighted variants need their own derivation.

Runtime dispatch must test CPU/OS support and retain a scalar path. Avoid `-march=native` for binaries distributed to unknown CPUs. Profile optimized builds with symbols; dead-code elimination can erase a microbenchmark. Test lifetime with ASan/UBSan, races with appropriate tooling, and allocator/thread overhead under realistic concurrency. A faster kernel that changes the model's quality or physical conservation beyond tolerance is not a valid optimization.

## Primary references

Check the documentation for the deployed version when an API, support matrix or policy matters. These are reference entry points, not permission to install or deploy.

- https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- https://www.open-std.org/jtc1/sc22/wg21/
- https://clang.llvm.org/docs/UsersManual.html
