# Remotion patterns

Use real imports and local helpers. This small fade/translation needs only the project's installed Remotion package:

```tsx
import {interpolate, useCurrentFrame, useVideoConfig} from 'remotion';

export const Label = ({text}: {text: string}) => {
  const frame = useCurrentFrame();
  const {fps} = useVideoConfig();
  const progress = interpolate(frame, [0, Math.max(1, Math.round(.4 * fps))], [0, 1],
    {extrapolateLeft: 'clamp', extrapolateRight: 'clamp'});
  return <div style={{opacity: progress, transform: `translateY(${24 * (1 - progress)}px)`}}>{text}</div>;
};
```

Inside a Sequence, confirm whether a cue is scene-relative or absolute. Convert seconds to frames in one place and reject missing cue names. Pure helpers should handle frame zero, holds, the final frame and seeks in arbitrary order.

Use SVG for labelled diagrams and Canvas for dense fields after measuring the bottleneck. Clear Canvas on every frame; use the project's frame-aware effect and asset-readiness mechanism. Three.js is justified by actual depth/spatial relationships, not by the availability of a package.

Compute audio envelopes and cue positions from the actual soundtrack. Keep volume automation a pure function of frame. Captions require checked timestamps; text divided by a guessed speaking rate is only a draft.

Render using existing project scripts or the locally installed CLI. Check the entry file and composition ID before running a full render; first inspect a still and a short range. If a font, codec or dependency is missing, fix the specific prerequisite. Do not invoke nonexistent harness video APIs or reinstall the entire stack as a generic retry.

Primary references: [Remotion interpolate](https://www.remotion.dev/docs/interpolate), [useCurrentFrame](https://www.remotion.dev/docs/use-current-frame), [rendering](https://www.remotion.dev/docs/render).
