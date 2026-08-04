# Milestone 4G — native Luanti material and drawtype renderer

Milestone 4G replaces the first native renderer's custom exposed-cube conversion with Luanti's own client mapblock mesh pipeline. Construct transforms remain independent scene parents, but the section geometry and materials now come from the same node definitions and mesh generator used by the ordinary world map.

## Section-to-mapblock staging

Each construct section is already 16×16×16 nodes, matching Luanti's mapblock size. For every dirty section the client now:

1. creates `MeshMakeData` at the section's ship-local block coordinate;
2. fills the mesh generator's surrounding voxel area with lit air;
3. copies construct nodes, `content_id`, `param1` and `param2` into the ship-local voxel manipulator;
4. resolves stale or unknown content IDs by registered node name;
5. constructs a normal `MapBlockMesh` through the active client, node-definition, texture and shader managers;
6. attaches each material layer as a child of the vessel's transformed scene root.

Neighbouring sections are still marked dirty when a boundary node changes, so hidden faces and connected geometry can be rebuilt across section borders.

## Rendering inherited from Luanti

Reusing `MapBlockMesh` provides the registered node's actual visual behaviour rather than maintaining a second partial renderer. The path includes:

- base and overlay tile layers;
- texture modifiers and texture-pack overrides already resolved by the client;
- palette and tile colours;
- facedir, 4dir and wallmounted orientation;
- fixed, wallmounted, leveled and connected nodeboxes;
- normal, allfaces, glasslike, framed glass, plantlike, torchlike, signlike, rail, liquid and mesh drawtypes supported by Luanti's generator;
- material and shader selection from client `NodeVisuals`;
- hidden-face removal and mesh-buffer merging;
- tile animation state retained by `MapBlockMesh`;
- packed day/night lighting and face shading.

M4G therefore removes the hard-coded `no_texture.png` material from the native vessel path.

## Animated materials

Each retained section owns its `MapBlockMesh`. During the client scene step, the section mesh receives the current animation time and day/night ratio through `MapBlockMesh::animate`. Animated registered tiles therefore advance without rebuilding construct geometry.

## Transparency bridge

Luanti normally renders transparent mapblock triangles through `ClientMap`'s sorted partial-buffer path. A moving construct is an independent scene parent, so M4G adds `MapBlockMesh::materializeTransparentBuffersForSceneNode`. It writes the stored transparent triangle indices back into their mesh buffers before the section is attached to Irrlicht.

This makes glass, alpha-clipped and alpha-blended vessel nodes visible. The current bridge sorts at scene-node/material granularity rather than re-sorting every transparent triangle against the camera each frame. That is a known quality limitation for overlapping transparent surfaces, not a missing-material fallback.

## Lighting boundary

- Node `param1` lighting captured at launch or sent in later section updates is preserved.
- Empty space around a staged construct section is treated as fully lit air so exposed hull faces do not render black.
- Luanti's shader material and day/night inputs remain active.
- Dynamic relighting from nearby world lights, construct self-shadow propagation and sunlight occlusion by a rotating vessel are not yet recalculated in real time.

## Integration changes

- `ClientConstructScene` now owns a `Client` reference rather than only a scene manager and texture source.
- `ClientConstructState` exposes received section data to the Luanti mesh adapter.
- The engine overlay adds a small public `MapBlockMesh` helper for scene-node transparency.
- The overlay fixture now verifies that the upstream mapblock header and implementation are patched exactly once.
- A dedicated client-scene compatibility target compiles the M4G renderer against typed Luanti API fixtures.

## Verification

- Release build with `-Wall -Wextra -Wpedantic -Werror`.
- Native construct regression tests.
- Client scene and packet integration compatibility compilation.
- Overlay application twice without duplicate changes.
- Lua gameplay smoke test.
- Storefront-neutral platform build.
- AddressSanitizer and UndefinedBehaviorSanitizer pass.
- ZIP integrity validation.

## Remaining boundary

- The complete modified Luanti executable still requires a full upstream-source link and live client/server runtime pass.
- Per-frame camera-relative sorting for intersecting transparent construct surfaces is not yet implemented.
- Dynamic world-to-construct relighting and shadow-map invalidation are pending.
- Construct mesh rebuilds are synchronous on the client; worker-thread scheduling and upload budgets belong to the optimisation pass.
- Camera-offset handling for extremely distant constructs still needs a live large-coordinate test.

The next engine slice is M4I: construct audio emitters, particles and visual effects, followed by persistence recovery and native launch/dock transaction recovery.
