# Orientation and loops

## Quaternion conventions and shortest arcs

State component order; this skill's helper uses [x,y,z,w]. Normalize finite nonzero inputs. Rotations q and -q are identical: if dot(q0,q1)<0, negate q1 before interpolation. Clamp the dot product to [-1,1] before acos. With theta=acos(dot), slerp weights are sin((1-u)*theta)/sin(theta) and sin(u*theta)/sin(theta). For very small theta use normalized linear interpolation to avoid cancellation. This follows [Shoemake's original quaternion animation paper](https://www.cs.cmu.edu/~kiranb/animation/p245-shoemake.pdf).

Derived example: identity=(0,0,0,1), 180 degrees around z=(0,0,1,0). Halfway is (0,0,sqrt(0.5),sqrt(0.5)), a 90 degree rotation. Interpolating identity against its negation should remain identity as a rotation, not pass through the zero quaternion. At exactly 180 degrees the two rotational directions are equally short; use a consistent policy or authored intermediate orientation. Multiple revolutions need explicit turns/keyframes; a single pair of orientations discards winding.

Slerp gives constant angular speed only for linearly advancing u within one segment. Adjacent slerps can jump angular velocity; use a quaternion spline or fit angular velocities when that matters. Multiplication order determines whether an increment acts in world or local coordinates. Test a known axis rotation before integrating a complicated hierarchy.

## Following a path without flips

A Frenet normal depends on curvature and becomes undefined on a straight segment or at an inflection. A fixed world-up look-at fails when forward aligns with up. For an oriented ribbon/camera, transport the previous frame by the smallest rotation aligning old and new unit tangents. Let axis=cross(t0,t1), sinAngle=|axis|, cosAngle=clamp(dot(t0,t1)). For a nonzero axis use atan2(sinAngle,cosAngle); reorthogonalize the normal against the new tangent and renormalize. For nearly parallel tangents keep the frame; for antiparallel tangents choose a stable perpendicular axis from the previous frame. Zero-length derivatives require holding/recovering orientation from a neighboring nondegenerate sample.

This construction is a local discrete transport rule; sample refinement still matters. Closed curves may accumulate a net twist even when tangents match. If the aesthetic demands a seamless frame, measure residual twist about the closing tangent and distribute its correction by arc length. That changes the transported frame deliberately. Preserve authored banking separately so a numerical seam correction does not erase it.

## Periodic position, velocity and acceleration

A seamless timed loop requires p(0)=p(T). If smooth velocity matters also require p'(0)=p'(T); smooth acceleration adds p''(0)=p''(T). Use tolerances in units, units/s and units/s². Rotation comparison uses |dot(q0,qT)|, not raw quaternion components; angular-velocity continuity needs a rotation-aware difference or explicit angular velocities.

A Fourier loop p(t)=center+sum[an*cos(2*pi*n*t/T)+bn*sin(2*pi*n*t/T)] with integer n has matching derivatives of all orders. Noninteger frequencies usually break the seam. For x=2*cos(2*pi*t/3), endpoint x=2, velocity=0, acceleration=-8*pi²/9 units/s² on both sides; matching acceleration need not mean zero acceleration.

For a move that holds before/after, quintic easing E(u)=6u⁵-15u⁴+10u³ gives E'(0)=E'(1)=E''(0)=E''(1)=0. Derived derivatives: E'=30u²(u-1)², E''=60u(2u²-3u+1). A 100-unit move in 2 s peaks at E'(0.5)*100/2=93.75 units/s. This creates smooth start/stop, not a repeating loop from endpoint back to start. A ping-pong version is position/velocity/acceleration continuous at reversals, but its jerk may jump.

Evaluate procedural noise using an explicit seeded periodic construction when exact loops matter; taking time modulo T does not make arbitrary noise continuous at T. For reproducible export sample frame i at i/fps and avoid exporting a duplicate endpoint frame unless the format specifically expects it.
