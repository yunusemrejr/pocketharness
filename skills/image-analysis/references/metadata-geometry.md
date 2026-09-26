# Metadata, geometry and bounded decoding

## Identify the evidence layer

A filename extension is a hint, not a decoder result. Start with a bounded header inspection when the question is dimensions or format. A successful header parse does not establish that the entire image decodes. For damaged or untrusted files, distinguish recognized format, complete decode and visual inspection in the report.

ImageMagick `magick identify input.png` reports basic format and geometry; verbose output includes additional metadata. Multi-frame files produce multiple image records. Consult [identify documentation](https://imagemagick.org/identify/) and the installed version before selecting formatting expressions. Avoid dumping all embedded profiles or comments when only dimensions are needed.

Bound both encoded file size and decoded resource requirements. A small compressed file can expand to a large raster or many frames. Apply time, memory, disk and pixel/frame limits appropriate to the task; codec delegates can execute substantial additional processing. Inspect the effective [ImageMagick security policy](https://imagemagick.org/security-policy/) rather than assuming a command-line memory limit overrides it. Unsupported format is a legitimate result, not a reason to weaken unrelated system policy.

## Declare the coordinate system

Record stored width and height, orientation metadata and displayed width and height separately. A portrait photo can store a landscape raster plus an orientation tag. If pixels are normalized for analysis, keep the original and record the transformation. For crop coordinates, say whether they refer to stored pixels or the oriented display. Choose and state a convention, such as x/y origin at the upper left with half-open bounds [x0,x1) × [y0,y1).

For an aspect-preserving fit inside W × H, the scale is min(W/w,H/h); for a fill-and-crop it is max(W/w,H/h). Round dimensions consistently and account for the resulting crop offset when mapping detections back. A bounding box measured on a thumbnail must be transformed back to source coordinates, with rounding uncertainty noted.

Animated images require frame selection and timing. A first-frame preview cannot establish the absence of later content. Composited frames and raw frame rectangles can differ because of disposal/blending behavior. State which representation was measured. Avoid multiplying file size by frame count as a decoded-memory estimate; dimensions, channels and working buffers matter.

## Compare like with like

Before pixel comparison, align orientation, dimensions, color representation and alpha handling. A byte hash proves identical bytes, while a decoded-pixel comparison proves equality only under the chosen decoder and transformations. Perceptual similarity is a different claim with a threshold requiring task-specific justification.

Keep measurement output small: source identifier, frame, geometry, transformation and result. Preserve the original for reproducibility. If comparing UI screenshots, separate layout movement from anti-aliasing or font differences rather than reporting every changed pixel as a functional defect.
