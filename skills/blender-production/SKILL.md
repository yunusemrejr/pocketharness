---
name: blender-production
description: "Build and verify Blender 3D scenes, procedural designs, rigged or physics animations and headless renders with bpy, reproducible assets and efficient render settings."
---

# Blender production

Inspect Blender version, scene units, existing assets and final deliverables before changing a scene. Preserve the editable blend file and external dependencies. Choose geometry, materials, camera and motion to support the requested shot; use physical units and a consistent coordinate convention.

Read [scene and motion](references/scene-motion.md) for procedural modeling, rigging and physics checks. Read [render pipeline](references/render-pipeline.md) for headless execution, render budgets, image sequences and export verification. Load only the relevant reference. Query installed bpy APIs and render engines instead of assuming current web examples match the local version.

Prefer explicit data APIs for repeatable scene construction; context-sensitive operators require a known mode and active selection. Name generated objects and collections deterministically so reruns update or replace owned objects without duplicating the scene. Do not remove unrelated user objects.

Validate a low-resolution preview and selected frames before a full render. Check silhouettes, camera clipping, shading, intersections, temporal continuity and lighting changes. For simulations verify units, collision margins and stable timesteps before increasing visual detail. Render long jobs to resumable image sequences and assemble delivery video separately. Deliver the editable scene and required assets alongside the requested render, with engine/version and any unverified visual or physical assumptions stated.
