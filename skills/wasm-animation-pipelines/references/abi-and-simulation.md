# ABI, state and simulation

## C boundary

Prefer a small versioned C ABI around C++ internals: create/destroy world, submit commands, advance fixed steps, obtain a render snapshot. Document scalar types, pointer width, byte offsets, alignment, counts, strides, units, coordinate handedness and error behavior. Do not expose `std::vector`, exceptions or C++ object layout to JavaScript. Catch errors at the boundary if the application uses exceptions; do not permit an unexpected throw to cross a C ABI.

A public entity ID can be a slot plus generation. After destroy/reuse, a stale generation must fail rather than operate on the next occupant. Define generation-wrap behavior; reject noninteger or out-of-range JS IDs before truncating. For a wasm32 ABI, pointers are byte offsets, while `HEAPF32` indexes are float elements; divide by four only after checking alignment. Memory64 changes JS pointer handling and must be an explicit ABI variant, not a flag silently applied to a wasm32 wrapper.

Validate `count <= capacity`, multiplication overflow and `bytes <= heapBytes - ptr` after validating `0 <= ptr <= heapBytes`. `_malloc` may fail; do not treat zero as an allocated array. Every allocation has one owner and is freed once by the matching allocator. Ownership transfer is not an instruction for both sides to free it. [Emscripten interop](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html).

## Views and snapshots

A borrowed `Module.HEAPF32.subarray(...)` is valid only while its allocation is alive, unmoved and not being concurrently overwritten. A C++ vector reallocation can move data even without Wasm memory growth. After a call that may allocate/grow, reacquire the pointer/length and current heap view. Unshared memory growth detaches old buffers; shared growth leaves older buffers with their old visible length. Do not cache views across those transitions. Use a copied snapshot when downstream async work needs stable ownership. A Wasm heap view does not eliminate a subsequent GPU upload. [Memory growth semantics](https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/JavaScript_interface/Memory/grow).

For output buffers owned by the caller, pass capacity and receive written/required lengths. If capacity is insufficient, return a documented status without partial undefined output; retry only with a bounded capacity policy. Make resize and destroy idempotent at the JS owner even if the low-level destroy function rejects duplicate handles.

## Fixed steps and render interpolation

Choose units explicitly, for example metres, seconds, kilograms. Keep `h` fixed; accumulate elapsed presentation time, advance while accumulator ≥ h, then interpolate previous/current snapshots with `alpha=accumulator/h`. This interpolation intentionally adds roughly one simulation-step latency. A maximum step count prevents a spiral of work, but decide whether excess elapsed time is dropped for an interactive effect or preserved for authoritative playback. A cap is not permission to claim simulated time equals wall time after a pause.

**Worked scheduling example.** For h=1/120 s and a 20 ms frame starting with no remainder, take two 8.333333 ms steps and retain 3.333333 ms: alpha=0.4. A moving point with previous/current x=0.016667/0.033333 m displays x≈0.023333 m. Do not extrapolate with alpha>1 after reaching a step cap; apply the declared backlog policy first.

**Worked integration example.** Semi-implicit Euler uses `v_next=v+a*h; x_next=x+v_next*h`. Dropping from rest with a=−9.81 m/s² and h=1/120 s gives v₁=−0.08175 m/s and x₁=−0.00068125 m; the analytic displacement is −0.000340625 m. This is expected first-step discretization error, not a unit conversion bug. Over one second, 120 steps give −4.945875 m instead of −4.905 m. Halving h halves this position bias for constant acceleration. Rendering interpolation does not improve solver accuracy.

For an undamped oscillator under semi-implicit Euler, `h*omega < 2` is the basic linear stability condition; contact constraints and coupled systems may impose tighter requirements. Tune solver iterations/substeps from error tolerances. Enforce normalized quaternions, finite state and bounded penetration; compare energy only where the model should conserve it. Damping, contacts and driving forces change the energy budget.

## Verification scenarios

Run identical command sequences through scalar C++ and Wasm with a documented numerical tolerance. Include zero entities, stale handles, insufficient output capacity, destroy/recreate, heap growth, vector growth and a render consumer retaining an old view. Compare equal simulation-step counts across frame schedules; do not compare different dropped-time policies as if they were equivalent. Treat NaN/Infinity as failure with entity and step diagnostics, not a reason to silently clamp all state.

Sources checked 2026-09-09. Worked examples and integration analysis are derived here.
