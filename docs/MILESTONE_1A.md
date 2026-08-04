# Milestone 1A — construct lifecycle prototype

## Purpose

M1A validates the full gameplay transition before committing to the final renderer:

```text
world nodes -> scanned snapshot -> active construct -> motion -> docking -> world nodes
```

## Native engine additions

The C++ layer now owns:

- construct identity and ownership;
- local voxel records;
- position and yaw;
- linear and yaw velocity;
- deterministic node enumeration;
- local bounds;
- versioned binary serialization;
- registry create/find/remove/import/step operations;
- server Lua API registration.

Construct IDs are exposed to Lua as strings so they remain exact after the registry exceeds Lua's safe integer range.

## Temporary renderer

`preview_constructs.lua` supplies a development-only renderer on stock Luanti. It removes the scanned nodes, persists their node/metadata snapshot, and creates one wield-item entity per node. A global step updates the construct transform and entity positions.

This renderer is deliberately capped at 512 nodes. It must not become the production solution because entity-per-node rendering, collision and networking scale poorly.

## Safety properties already included

- Complete node snapshot before removal.
- Immediate mod-storage save after launch.
- Restart reconstruction of active previews.
- Dock-space validation before replacing nodes.
- Metadata restoration during docking.
- Native registry rollback when preview launch fails.
- Serialization length and node-count limits.
- Truncated/corrupt payload rejection.

## Next engineering slice

M1B replaces the entity renderer with native construct sections:

1. 16x16x16 local construct sections.
2. Greedy or Luanti-compatible mesh generation per dirty section.
3. One scene node hierarchy per construct.
4. Server transform snapshots.
5. Client interpolation.
6. Native save/load connection.
7. Launch/dock journal with recovery states.

M1B acceptance target: a 10x4 raft rendered as native construct sections moves and turns smoothly for two clients, with no entity-per-block objects.
