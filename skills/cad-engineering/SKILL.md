---
name: cad-engineering
description: "Create, edit and validate parametric CAD models and engineering drawings using AutoCAD, FreeCAD, OpenSCAD or compatible tools; handle DXF/DWG/STEP and manufacturing geometry."
---

# CAD engineering

Establish units, coordinate system, dimensions, tolerances and intended use before modeling. Inspect existing drawings or model history and preserve the native source. Distinguish editable parametric solids, exchange geometry, 2D drawings and display meshes; they carry different information.

Read [parametric geometry](references/parametric-geometry.md) for constraints, dimensions, boolean operations and geometric checks. Read [exchange and drawings](references/exchange-drawings.md) for AutoCAD, FreeCAD, OpenSCAD, DXF/DWG/STEP and fabrication outputs. Load only the relevant reference. Choose an installed tool that preserves the user's required format and workflow; do not assume a mesh import reconstructs a design history.

Model design intent through meaningful dimensions, constraints and named parameters. Prefer stable datum references over incidental face indices when subsequent edits can change topology. Keep features and script outputs reproducible, and avoid changing unrelated drawing entities.

Recompute and inspect resulting geometry, then measure critical dimensions independently. Check validity, solid count, connectivity, interference and units appropriate to the deliverable. Render useful orthographic/isometric views, but do not treat a plausible screenshot as proof of manufacturability. Reopen exported files and compare bounds and required features. Deliver the native editable model plus requested exchange/drawing formats, recording assumptions and validation limits; generating CAD does not authorize fabrication or machine operation.
