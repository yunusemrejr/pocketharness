# Curves and timing

## Cubic shape and time are separate

For u in [0,1], cubic Bézier B(u)=(1-u)^3*P0+3*(1-u)^2*u*P1+3*(1-u)*u²*P2+u³*P3.

```
B'(u) = 3[(1-u)²(P1-P0)+2(1-u)u(P2-P1)+u²(P3-P2)]
B''(u) = 6[(1-u)(P2-2P1+P0)+u(P3-2P2+P1)]
```

These are derivatives with respect to u. For u=t/T: velocity=B'/T and acceleration=B''/T². More generally, with a time warp u(t), velocity=B'*uDot and acceleration=B''*uDot²+B'*uDDot. Smooth geometry alone does not eliminate timing acceleration.

Derived example: P0=(0,0), P1=(1,0), P2=(1,1), P3=(2,1), T=2 s. At u=0.5, position=(1,0.5), B'=(1.5,1.5), B''=(0,0). Speed is sqrt(1.125)=1.060660 units/s. Initial velocity=(1.5,0); initial acceleration=(-1.5,1.5). Doubling duration halves all velocities and quarters all accelerations.

For Hermite endpoints P0/P1 with endpoint velocities V0/V1 over T seconds, use tangents M0=T*V0 and M1=T*V1:

```
H(u)=(2u³-3u²+1)P0+(u³-2u²+u)M0
     +(-2u³+3u²)P1+(u³-u²)M1
```

Equivalent Bézier controls are P0, P0+M0/3, P1-M1/3, P1. Forgetting T converts a velocity requirement into an accidental parameter-space tangent.

## Joins

At cubic A followed by B, C0 means A3=B0. For equal parameter scales, C1 means A3-A2=B1-B0; C2 also requires A3-2A2+A1=B2-2B1+B0. For durations TA/TB use first derivatives divided by duration and second derivatives divided by duration squared. G1 requires nonzero endpoint tangent directions agree with positive scale, but speeds may differ. G2 concerns curvature continuity after reparameterization, not equality of second derivative vectors. Zero tangents need separate analysis.

Example: A's last control edge=(1,0), TA=1 s; B's first edge=(2,0), TB=2 s. Both join velocities are (3,0) units/s although parameter derivatives differ. SVG's S command reflects the prior cubic control point; that provides equal parameter tangents, not automatically equal timed velocity. Z closes with a straight segment and can introduce a corner. Path semantics are defined by the [SVG 2 path specification](https://www.w3.org/TR/SVG/paths.html).

For Catmull–Rom on uneven points, centripetal parameterization is often a better starting point than uniform spacing. Its no-cusp/no-self-intersection result is within individual curve segments under the paper's conditions, not a promise that a whole arbitrary closed path cannot cross. Handle repeated points explicitly. See [Yuksel, Schaefer and Keyser's parameterization analysis](https://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf).

## Arc-length timing and acceleration budget

s(u)=integral from 0 to u of |B'(v)|dv. For constant speed v, invert s(u)=v*t rather than advancing u linearly. Build a monotone lookup table with adaptive subdivision or numerical quadrature, then binary-search a bracket and refine. An optional Newton step is uNew=u-(s(u)-target)/|B'(u)|; keep it inside the bracket and fall back to bisection near zero speed. Uniform samples can miss tight bends; compare lengths/timing after refinement. A stationary/zero-length path must return a defined hold, not divide by total length.

Curvature in 3D is kappa=|B' cross B''|/|B'|³ where speed is nonzero. Constant-speed traversal still has normal acceleration v²*kappa. For a radius-2-unit circle and v=3 units/s, acceleration is 4.5 units/s². With acceleration budget 2, maximum constant speed there is sqrt(2*2)=2 units/s. A camera may use this as a comfort/art-direction budget rather than physical law. Changing coordinate scale changes curvature and speed units; nonuniform scaling requires recalculation in the rendered space.
