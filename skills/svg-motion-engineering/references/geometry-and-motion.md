# SVG geometry and motion

Use the relevant section: coordinates; curves; stroke reveals; morphing; bounds and validation. API sources checked 2026-09-09. Calculations below are worked derivations, not browser performance promises.

## Coordinates and moving pivots

Represent a 2D affine transform with column vectors: `x'=a*x+c*y+e`, `y'=b*x+d*y+f`. Composition `T*S*p` scales the point then translates it. SVG `transform="translate(100 20) scale(2)"` therefore maps `(3,4)` to `(106,28)`, whereas reversed operations map it to `(206,48)`. Test with one known point when mixing a library's matrix conventions with DOM matrices.

For pointer input, convert `clientX/clientY` using the inverse of the target geometry's `getScreenCTM()`, not a hand-built subtraction of its bounding rectangle:

```js
function localPointer(element, event) {
  const matrix = element.getScreenCTM();
  if (!matrix) return null;
  const inverse = matrix.inverse();
  const p = new DOMPoint(event.clientX, event.clientY).matrixTransform(inverse);
  return Number.isFinite(p.x) && Number.isFinite(p.y) ? p : null;
}
```

A singular transform, detached element, or scale approaching zero needs a defined interaction fallback. A non-invertible matrix produces NaN components, which the guard rejects. Do not use `pageX` here without accounting for scroll. Cache matrices only while ancestors, scroll, viewport and transforms remain unchanged. See [getScreenCTM](https://developer.mozilla.org/en-US/docs/Web/API/SVGGraphicsElement/getScreenCTM) and [matrix inversion](https://developer.mozilla.org/en-US/docs/Web/API/DOMMatrixReadOnly/inverse).

For a `viewBox=(vx,vy,vw,vh)` and viewport `(W,H)`, `xMidYMid meet` uses `s=min(W/vw,H/vh)`, `tx=(W-s*vw)/2-s*vx`, `ty=(H-s*vh)/2-s*vy`. `slice` uses the maximum and crops; `none` uses independent axis scales. Example: `viewBox="10 20 200 100"`, viewport `600×400`: `s=3`, `(tx,ty)=(-30,-10)`. Point `(110,70)` maps to `(300,200)`. A viewport point `(450,260)` maps back to `(160,90)`. These coordinates exclude any outer page offset; the CTM includes the actual nesting. [SVG viewport mapping](https://www.w3.org/TR/SVG2/coords.html#ComputingAViewportsTransform).

Rotate a joint about local pivot `c` with `T(c)*R(theta)*T(-c)`. Keep a group per joint so parent rotations move children coherently. CSS transform origins can refer to a different box from SVG user-space coordinates; set and verify `transform-box`/`transform-origin`, or use explicit group matrices. Avoid letting CSS and attribute animations independently own the same transform.

## Arc length, speed, and tangent orientation

For cubic control points `P0..P3`, `B(u)=(1-u)^3 P0+3(1-u)^2u P1+3(1-u)u² P2+u³ P3`. Velocity in parameter space is `B'(u)=3(1-u)²(P1-P0)+6(1-u)u(P2-P1)+3u²(P3-P2)`. Distance is `s(u)=integral[0,u] |B'(q)| dq`. To travel at speed `v`, evaluate `B(s^-1(v*t))`; easing modifies distance, not the geometric parameter.

For a DOM path, measure `L=path.getTotalLength()` after geometry updates, then evaluate `path.getPointAtLength(clamp(v*t,0,L))`. `t` is seconds if `v` is user units/second. A 240-unit path at 80 units/s takes 3 seconds; at 0.9 seconds the requested distance is 72 units. [Path point sampling](https://developer.mozilla.org/en-US/docs/Web/API/SVGGeometryElement/getPointAtLength).

For custom curves, construct a monotonic `(u,s)` lookup table by adaptive subdivision or numerical quadrature, binary-search the distance, then refine within the bracket. Specify an absolute screen-space tolerance and maximum subdivision depth. A coarse table can miss a tight loop even if endpoints coincide; a control-polygon/chord error estimate is better than midpoint-only detection. Duplicate distances require a zero-length policy; do not divide by their difference. Newton refinement `u -= (s(u)-target)/|B'(u)|` needs a bracket and a derivative guard at cusps.

Nonuniform scaling breaks constant screen speed: with scale `(2,1)`, a local horizontal speed 80 becomes 160 CSS px/s while a vertical speed 80 stays 80. Build the table from transformed points or integrate `|A B'(u)|` when screen speed matters. Translation does not affect speed; a time-varying transform adds its own velocity and invalidates a static transformed table.

Orient with `atan2(dy,dx)` from analytic derivatives or a symmetric distance difference `P(s+epsilon)-P(s-epsilon)`. Clamp at endpoints; a cusp has no unique tangent, so preserve the previous heading or use an authored turn. Unwrap angles before interpolation to avoid an unintended nearly-full rotation across `-pi/pi`. For a closed loop, test position and tangent continuity at the seam separately.

## Drawing with strokes

For a single reveal use a numeric measured length `L`, `stroke-dasharray: L L` and animate `stroke-dashoffset` from `L` to `0`. Alternatively normalize with `pathLength="1"` and numeric dash values; verify this path-length calibration in the target browser. Percentage dash values are not “percent of this path”: they relate to the viewport's normalized diagonal. Odd-length dash lists repeat to make an even list. See [dash-array semantics](https://developer.mozilla.org/en-US/docs/Web/SVG/Reference/Attribute/stroke-dasharray) and [path length calibration](https://www.w3.org/TR/SVG2/paths.html#PathLengthAttribute).

Rounded caps extend a visible dash beyond its geometric endpoints by roughly half the stroke width. A 10-unit stroke can show about 5 units past the reveal tip; a zero-length dash with round caps can show a dot. Use butt caps or a separate visibility/mask phase when “fully hidden at time zero” is required. Multiple subpaths restart dash placement: split them into paths for sequential writing, and budget duration from each measured length. Check joins and closed-path seams at completion.

## Morph topology

Direct path-data interpolation needs compatible command structures; equal array length alone is insufficient. Normalize relative commands, pair subpaths deliberately, align closed-path start points and winding, and preserve holes/fill-rule meaning. When resampling, sample arc length, then compare orientation and cyclic shifts to minimize correspondence distance. Uniform point interpolation can still self-intersect or collapse thin features; inspect curvature and area through time. SVG defines path interpolation constraints in [path data](https://www.w3.org/TR/SVG2/paths.html#PathData).

For a constrained morph, distinguish boundary correspondence from physical material motion. Volume/area preservation does not follow from interpolation. Track signed polygon area `A=0.5*sum(x_i*y_(i+1)-x_(i+1)*y_i)` as an approximate diagnostic, separately per hole. A sign flip flags inverted winding; shrinking area may be intentional. Do not label every change an error. For different topology, a masked transition or crossfade often gives a clearer result than fabricated vertex correspondence.

## Bounds, measurements, and failure checks

A blur with standard deviation `sigma=4` has a practical visual margin of about `3*sigma=12` user units on each side (Gaussian tails are infinite; this is a chosen truncation). Add stroke, shadow offset and displacement margins. For a box `(0,0,100,40)` with stroke width 6 and blur sigma 4, a conservative symmetric illustrative margin is `3+12=15`, yielding `(-15,-15,130,70)`. With `filterUnits="userSpaceOnUse"`, specify that region in the intended coordinate system; default object-box fractions behave differently for tiny/degenerate objects. Enlarging bounds increases raster work. [Filter coordinate units](https://developer.mozilla.org/en-US/docs/Web/SVG/Reference/Attribute/filterUnits).

Measure geometry once per change, batch writes, and profile paint/raster time for filter-heavy scenes. Transforming an SVG is not proof it is composited cheaply. Inspect a browser trace with the actual instance count and display scale; record browser, hardware, viewport, DPR, warmup and p95 frame interval. Test start/mid/end/seam, zero length, sharp cusp, nested transforms, resize mid-motion, detached/reinserted SVG, reduced motion, transparent-background export, and filter clipping. Pick tests relevant to the geometry changed rather than adding all cases to every task.
