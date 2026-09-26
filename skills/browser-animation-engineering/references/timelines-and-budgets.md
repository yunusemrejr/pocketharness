# Browser animation timelines and budgets

Sections: clocks; interruption; FLIP; budgets; deterministic capture. API sources checked 2026-09-09. Numeric examples are derived estimates to verify against the target browser.

## Clock ownership and lifecycle

Use the RAF callback's timestamp for progression: `elapsed=(timestamp-start)/1000`. Multiplying a per-frame increment by a guessed 60 fps fails on 120 Hz displays. Initialize `start` from the first callback to avoid a jump after scheduling delay. Multiple callbacks in the same frame share a timestamp; a central loop avoids competing owners and redundant work. RAF is one-shot and commonly paused in hidden tabs. [requestAnimationFrame](https://developer.mozilla.org/en-US/docs/Web/API/Window/requestAnimationFrame).

Define resumption semantics. A decorative timeline can jump to absolute elapsed time; a paused interaction can preserve elapsed active time; a physics simulation should cap accumulated work and document any dropped time. Do not silently feed a multi-second step into an integrator. Store RAF IDs with `null` as the “not scheduled” sentinel, cancel on teardown, and avoid starting another loop during a resize or replay. Disconnect observers and remove media-query/event handlers owned by the animation.

For WAAPI, hold the returned `Animation`; use its `currentTime` for seeking in milliseconds, considering that unresolved time can be `null`. Await `ready` where a pending pause/play operation matters. Multiple animations driven by a document timeline can be aligned deliberately; setting different start times sequentially from `performance.now()` is not exact synchronization. [Animation currentTime](https://developer.mozilla.org/en-US/docs/Web/API/Animation/currentTime).

Finishing and cancellation have different meanings. Handle cancellation of `finished` promises to avoid unhandled rejection, but do not swallow errors from unrelated work. For an interrupted effect, `commitStyles()` can preserve the current computed effect before cancellation when supported and applicable; it writes inline styles, so account for responsive CSS and stylesheet ownership. For a settled endpoint, often set the semantic class/state, then cancel the transient animation. A forever-filling animation can retain effects and make later style changes confusing. [commitStyles](https://developer.mozilla.org/en-US/docs/Web/API/Animation/commitStyles).

## Retargeting without position or velocity jumps

Sample current position from the animation model if possible; a computed transform alone does not encode velocity or rotational winding. Restarting an ease-in curve from the sampled position maintains C0 position continuity but usually breaks C1 velocity continuity.

Use a cubic Hermite segment when target time and endpoint velocity are prescribed. Let `u=t/T`, `h00=2u³-3u²+1`, `h10=u³-2u²+u`, `h01=-2u³+3u²`, `h11=u³-u²`:

`x(t)=h00*x0+h10*T*v0+h01*x1+h11*T*v1`.

Example: interrupted motion at `x0=40px`, `v0=120px/s`, new target `x1=100px`, `v1=0`, duration `T=0.5s`. At half time `u=.5`, `x=.5*40+.125*.5*120+.5*100=77.5px`; derivative is `150px/s`. Position and velocity are continuous at the join, but acceleration is not generally continuous. Cubics can overshoot if initial speed is large or opposite-directed; respect bounds by choosing duration/trajectory rather than abruptly clamping velocity.

A critically damped spring is useful when duration need not be exact. For displacement `y=x-target`, angular rate `omega>0`, constant target, `y(t)=(y0+(v0+omega*y0)*t)*exp(-omega*t)`. Its derivative is `(v0-omega*(v0+omega*y0)*t)*exp(-omega*t)`. At each target change evaluate both current position and velocity, then begin the new analytic segment. `omega` has units 1/s; it is not a duration. Terminate when BOTH displacement and speed are below product-specific tolerances. This avoids a final moving snap. The formula is a scalar model, not a substitute for contact constraints or rigid-body simulation.

An easing curve defines progression `e(u)`; velocity is `(x1-x0)*e'(u)/T`. For CSS cubic-bezier timing, the input is the curve's x coordinate, so invert x before evaluating y; using Bézier parameter as time changes the easing. For angle retargeting preserve chosen winding, and for 3D rotations use quaternion interpolation rather than independent Euler components.

## FLIP and coordinate caveats

Read First rectangles, apply final layout once, read Last rectangles, then animate an inverse visual transform to identity. Batch all reads for a phase; interleaving each item's read/write can force repeated layouts.

With axis-aligned elements in the SAME coordinate system and transform-origin `0 0`, use `dx=first.left-last.left`, `dy=first.top-last.top`, `sx=first.width/last.width`, `sy=first.height/last.height`. Example: first `(20,30,100,40)`, last `(80,50,200,80)` gives `translate(-60px,-20px) scale(.5,.5)` as the starting visual transform. The final layout owns geometry while the transform produces the old appearance.

Viewport rectangles become insufficient with rotated/skewed ancestors, changing scroll, nested transforms, perspective or pre-existing transforms. A viewport displacement may not equal a local translation: convert through the ancestor matrix or animate an overlay in a known coordinate space. Capture the same scroll state or incorporate its delta; guard zero-size Last rectangles. Scaling text can distort it during the transition; animate a wrapper, use a size-aware technique, or accept that effect deliberately. Do not overwrite an existing transform—compose with a wrapper or explicit matrices and inspect transform order.

Read geometry with [getBoundingClientRect](https://developer.mozilla.org/en-US/docs/Web/API/Element/getBoundingClientRect); it describes the viewport-relative bounding rectangle, not an inverse mapping for arbitrary transformed content.

## Frame deadlines, raster density, and resize

At 60 Hz the nominal interval is `1000/60=16.67ms`; at 120 Hz it is `8.33ms`. These are TOTAL frame intervals, not JavaScript allowances. Suppose measured work at 120 Hz is 2ms scripting, 1.5ms style/layout, 3ms raster and 1ms other work: the 7.5ms sum leaves only .83ms illustrative headroom. Pipeline stages can overlap; a trace establishes the actual bottleneck. Reducing script by .2ms does little if raster is dominant.

Record RAF interval p50/p95/max and trace style/layout/paint/GPU activity under representative interaction. A callback timer measures only callback CPU time. Long Animation Frames entries cover frames above 50ms, so their absence does not prove 60 or 120 Hz smoothness. Feature-detect entry support; also inspect ordinary missed deadlines. [Long animation frame timing](https://developer.mozilla.org/en-US/docs/Web/API/Performance_API/Long_animation_frame_timing).

For canvas set CSS size separately from backing dimensions: `width=round(cssWidth*dpr)`, `height=round(cssHeight*dpr)` and reset the context transform after resizing. Assigning dimensions resets canvas state; restore drawing configuration. If a DPR cap is used, document it as a quality/performance choice. DPR can change with browser zoom or moving between displays, not just CSS resize. [devicePixelRatio](https://developer.mozilla.org/en-US/docs/Web/API/Window/devicePixelRatio).

Example: a `1200×800` CSS pixel surface at DPR 2 has `2400×1600=3,840,000` pixels, requiring `15,360,000` bytes, about `14.65 MiB`, for ONE RGBA8 buffer. DPR 3 gives `8,640,000` pixels and `32.96 MiB`, 2.25 times the area. Actual GPU consumption also includes additional buffers, depth/stencil, textures and implementation allocation. A resize handler should update size/projection once per needed change; avoid observer loops caused by writing the same dimension being observed. Rebuild expensive geometry only if dependent inputs changed.

Prefer transform/opacity for ordinary composited motion where they produce the intended visual result, but verify promotion and raster behavior. Broad `will-change`, large translucent layers, animated blur, and many offscreen surfaces can worsen memory/bandwidth. [CSS and JavaScript animation performance](https://developer.mozilla.org/en-US/docs/Web/Performance/Guides/CSS_JavaScript_animation_performance).

## Deterministic seeking, capture, and accessible endpoints

Expose `renderAt(tSeconds)` for procedural animation or pause WAAPI and set `currentTime=tSeconds*1000`. Clamp a non-looping timeline explicitly; for loops use positive modulo and define whether a requested endpoint represents the seam or the last full frame. For frame rate `F`, export frame `k` at `t=k/F`, independent of rendering speed. A two-second 30fps half-open sequence has 60 frames at `0..59/30`; adding the endpoint creates 61 frames. Choose intentionally.

Freeze randomized inputs with a seed, resolve fonts/images before capture, set viewport/DPR, and await application rendering readiness. A RAF callback runs before paint; it alone does not prove a screenshot has captured a finished GPU frame. Use the capture system's rendering synchronization. History-dependent simulations need reset+fixed-step replay or validated checkpoints; assigning a timestamp does not rewind their state.

Use `prefers-reduced-motion` for a stable semantic endpoint or an appropriate small non-spatial transition. Apply preference changes while running, preserve focus and state, and avoid “paused at invisible first frame.” Reduced motion does not mean removing confirmation or changing data. [Reduced motion preference](https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/At-rules/@media/prefers-reduced-motion).

Verify rapid reversals, interruption near endpoints, resize during motion, hidden-tab resume, component removal, repeated mounting, seeking backward, 0-duration motion and preference changes. Choose a small representative subset for the implementation. Report what ran in a browser versus what was only calculated; neither source documentation nor arithmetic is evidence of rendered correctness.
