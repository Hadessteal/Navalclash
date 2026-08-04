# Milestone 4A — native smooth-construct core

This milestone continues the engine work without changing or guessing the NavyCraft gameplay specification. The supplied NavyCraft and Shipyard JARs remain authoritative for game mechanics; this milestone is limited to engine capabilities needed to render, network, collide and interact with those vessels smoothly.

## Implemented

### Section lifecycle

- Constructs remain in integer ship-local voxel coordinates.
- Nodes are partitioned into 16x16x16 sections.
- Rebuilds produce deterministic section order.
- Incremental node edits update only the owning section.
- Edits on a section boundary also mark the adjacent section dirty because face visibility can change across the boundary.
- Each section and construct node store has a monotonic revision.

### Section meshing

- Hidden faces between adjacent construct nodes are removed.
- Visible cube faces are emitted as indexed triangles.
- Vertices contain local position, local normal and UV coordinates.
- Geometry is grouped by node-name material so an Irrlicht adapter can assign the appropriate node texture/material later.
- Mesh generation works across section boundaries because neighbour checks use the complete construct voxel store.

This is a geometry core, not the final Luanti visual adapter. It does not yet reproduce all Luanti node drawtypes, connected textures, lighting, animation, liquids, glass sorting or facedir-specific meshes.

### Transform replication

- Versioned transform packet codec.
- Construct ID, sequence number and server timestamp.
- Position, yaw, linear velocity and yaw velocity.
- Ordered client snapshot buffer.
- Rejection of stale, duplicate, cross-construct or backwards-time snapshots.
- Shortest-path yaw interpolation.
- Configurable interpolation delay.
- Capped extrapolation when a new snapshot is late.

### Section replication

- Versioned section packet codec.
- Construct ID, section coordinate and revision.
- Node coordinates, content ID, parameters, name and metadata.
- 4,096-node section limit.
- 1 MiB per-string limit.
- 16 MiB total packet limit.
- Truncation, bad magic, unsupported version and trailing-data rejection.

### Client reconstruction

`ClientConstructManager` accepts transform and section packets and reconstructs a client-local `DynamicConstruct`. It maintains:

- ordered transform snapshots;
- current local voxel state;
- section revisions;
- dirty mesh sections;
- cached section meshes;
- client-side construct raycasts.

This provides the data side of the client renderer. The next slice must convert `ConstructSectionMesh` buffers into Irrlicht mesh buffers and attach them to a transformed scene node.

### Collision and interaction geometry

- Rotated world bounds for yaw-only constructs.
- Broad-phase world AABB rejection.
- Exact yaw-only voxel OBB versus AABB checks using separating axes in the XZ plane plus Y overlap.
- Swept AABB movement using bounded stepping and binary collision-time refinement.
- Construct-local Amanatides-Woo DDA raycasting.
- World hit position, local hit position, node coordinate, normal and distance.

### Moving-platform contact

A platform contact stores a ship-local anchor. Every update computes:

- the new world anchor;
- displacement since the previous transform;
- shortest yaw delta;
- measured surface velocity.

This is the core required to move a player with a translating and turning deck without teleporting every individual ship block. The next slice must connect it to Luanti's local and server player movement code, contact detection and reconciliation.

### Server Lua API additions

- `core.raycast_dynamic_constructs(start, finish)`
- `core.get_dynamic_construct_surface_velocity(id, world_point)`
- `core.set_dynamic_construct_node(id, local_pos, node_or_nil)`
- `core.get_dynamic_construct_sections(id)`

## Verification

- C++17 Release build with warnings treated as errors.
- Native core tests pass.
- AddressSanitizer and UndefinedBehaviorSanitizer pass.
- Server Lua API translation unit compiles against the compatibility stubs.
- Storefront-neutral adapter builds.
- Luanti overlay fixture applies twice without duplicating modifications.
- Complete Milestone 3 Lua game smoke test still passes.
- 5,000-node benchmark completes sectioning, meshing, packet creation and 10,000 construct raycasts.

Example benchmark from this build environment:

```text
nodes=5000 sections=4 visible_faces=1900 section_packet_bytes=190152 build_partition_ms=2 mesh_packet_ms=2 raycasts=10000 ray_hits=9600 raycast_ms=18
```

Timing is machine-dependent and is not a performance guarantee.

## Next integration slice

1. Add NavyCraft client packet commands to Luanti's protocol enum and command table.
2. Add server broadcast methods for create/remove, section data and transform snapshots.
3. Add client packet handlers feeding `ClientConstructManager`.
4. Convert `ConstructSectionMesh` into Irrlicht mesh buffers.
5. Create one transformed scene parent per construct and one mesh child per section.
6. Connect local-player and server-player collision to `ConstructGeometry` and `PlatformContact`.
7. Merge construct raycast hits with ordinary world/object raycast selection.
8. Build and run the complete modified Luanti client and dedicated server.
