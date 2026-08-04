# Milestone 4B — Luanti packet dispatch and native scene integration

This milestone wires the Milestone 4A construct core into the Luanti 5.16.1 client/server architecture. It does not change the NavyCraft gameplay rules derived from the supplied JARs.

## Implemented

### Protocol commands

Four server-to-client commands are reserved after Luanti 5.16.1's stock command range:

- `0x65` construct section
- `0x66` construct transform
- `0x67` construct removal
- `0x68` construct reset

The overlay updates `TOCLIENT_NUM_MSG_TYPES` to `0x69` and adds connected-state client dispatch handlers.

### Server transmission

`Server::SendNavyCraftConstructMessage` sends reliable section/removal/reset data and unreliable transform snapshots. Broadcasts create a fresh `NetworkPacket` for each connected client.

The native Lua API now broadcasts automatically when a construct is:

- created;
- moved or given a new velocity;
- stepped by the server;
- edited locally;
- removed.

Transform and section sequences are monotonic for the life of each server construct.

### Client packet handlers

The custom handlers validate and apply the versioned packet payloads to `ClientConstructManager`. Invalid, truncated, stale and duplicate packets are rejected without crashing the client.

The client resets all construct scene state when reconnecting and when a reset packet is received.

### Irrlicht scene adapter

`ClientConstructScene` now:

1. reconstructs ship-local voxel sections from packets;
2. rebuilds only dirty section meshes;
3. converts exposed-face geometry into Irrlicht `SMeshBuffer` objects;
4. splits large material buffers before the 16-bit vertex-index limit;
5. places section mesh nodes under one parent scene node per construct;
6. samples the transform snapshot buffer each client frame;
7. applies smooth position and yaw to the parent;
8. removes all child nodes when the construct is removed.

The first adapter deliberately uses `no_texture.png`. Proper Luanti node material, drawtype, lighting, facedir, glass and animated-texture support remains a later rendering pass.

### Game renderer selection

The engine exposes `core.get_dynamic_construct_protocol_version()`. The game uses the native renderer when that function is present and returns at least version 1. Stock Luanti continues to use the temporary entity preview.

## Verification performed

- Release C++17 core build with warnings as errors.
- Native unit tests passed.
- AddressSanitizer and UndefinedBehaviorSanitizer passed.
- Native Lua API translation unit compiled against compatibility stubs.
- Overlay fixture patched a Luanti-shaped tree twice without duplicate edits.
- Storefront-neutral platform adapter built.
- 5,000-node benchmark completed.
- ZIP integrity and SHA-256 checks performed during packaging.

## Not yet verified

The complete modified Luanti client and dedicated server were not compiled in this environment because the full upstream source tree and client dependencies were not locally available. The patcher is anchored to Luanti 5.16.1 source locations, but the first real Windows/Linux engine build may expose API or include adjustments in the new Irrlicht adapter.

Lua runtime smoke testing was also unavailable in this environment because no Lua or TeX Lua interpreter was installed. All Lua files were retained from M3 except the native-renderer selection path.

## Next engine slice

Milestone 4C should connect:

- construct collision to local and server player movement;
- persistent platform-contact anchors;
- jump and landing velocity inheritance;
- server reconciliation for riders;
- construct raycast results to ordinary dig/place/use interaction;
- late-joining client full-state synchronisation;
- proper node materials and lighting in the native mesh adapter.
