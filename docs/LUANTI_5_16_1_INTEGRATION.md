# Luanti 5.16.1 native integration map

This document records the next source-level integration work. It is intentionally separate from the tested engine-independent core so unfinished Luanti hooks are not confused with working code.

## Client-bound protocol commands

Reserve the NavyCraft `ToClientCommand` range after the current final 5.16.1 command:

- construct create/remove lifecycle;
- construct section full/delta data;
- construct transform snapshot;
- construct removal and scene reset;
- construct-attached sound and particle effects.

Required upstream files:

- `src/network/networkprotocol.h`: command values and packet layouts;
- `src/network/clientopcodes.cpp`: command-table entries;
- `src/client/client.h`: handler declarations and `ClientConstructManager` ownership;
- `src/network/clientpackethandler.cpp`: decode packet byte strings and feed the manager.

All construct lifecycle and section packets must share the same reliable ordered channel. Transform snapshots may use an unreliable channel only after lifecycle ordering is guaranteed and stale sequence rejection remains active.

## Server broadcasting

Add server methods that:

1. send construct create and section data when a client enters interest range;
2. send changed sections only when their revision changes;
3. send transform snapshots at a configured frequency;
4. send construct removal when destroyed, docked or out of interest range.

The packet codec in `construct_packets` is bounded and independent of Luanti's `NetworkPacket`; the adapter can write its encoded bytes as a length-prefixed payload.

## Client scene graph

Create one scene parent per construct. The parent owns the sampled position and yaw. Each 16x16x16 section is staged as ship-local voxel data and passed through Luanti's existing `MapBlockMesh` generator.

The M4G visual adapter now:

- resolves content IDs and fallback node names through `NodeDefManager`;
- uses Luanti's normal texture, shader, palette, facedir, nodebox and drawtype pipeline;
- retains both tile layers and animated material state;
- materialises transparent triangle indices for independent scene nodes;
- rebuilds only dirty sections and their boundary neighbours;
- removes child nodes when section-removal packets arrive;
- updates the parent transform every render frame from `ClientConstructState::sampleTransform`.

The generic cube mesher remains an engine-independent fallback and test utility. Dynamic world relighting, per-frame transparent triangle sorting and asynchronous mesh uploads remain later integration work.

## Player physics

Both the server and local client need the same contact state:

- selected construct ID;
- local deck anchor;
- previous construct transform;
- support-node/local contact data.

The movement loop should apply platform displacement before player input collision, then resolve the player's own movement against construct voxels and world nodes. Jump velocity should inherit the platform surface velocity at the contact point.

## Interaction selection

Run the ordinary Luanti raycast and `ClientConstructManager::raycast` over the same segment. Choose the nearest valid hit. Extend interaction serialisation with construct ID and local node coordinate rather than pretending the hit is a stationary world node.

Server validation must repeat the construct raycast or otherwise validate range, target revision and permissions before performing a local node edit.

## Persistence and docking

Native world persistence still needs:

- per-construct files or database records;
- an atomic launch journal before world nodes are removed;
- an atomic docking journal before construct storage is deleted;
- restart recovery for incomplete launch or docking transactions;
- section revisions preserved across restart.

## Construct effects

M4H adds command `0x69` for versioned construct-local effect events. `ClientConstructEffects` uses the reconstructed vessel transform to keep looped positional sounds and continuous particle emitters attached while the vessel moves. It reuses Luanti's `ISoundManager` and `ParticleManager` rather than introducing a second audio or particle renderer.

Effects are removed when their construct is removed and are cleared on scene reset. Interest-range filtering and high-player-count bandwidth tuning remain later work.


## M4I projectile integration

M4I adds `TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE` at `0x6A` and advances `TOCLIENT_NUM_MSG_TYPES` to `0x6B`. The server broadcasts unreliable position updates and reliable spawn, impact and removal events. The client routes packets into `ClientConstructEffects`, which keeps a stale-resistant projectile state and creates trail and impact effects.

Construct collision and vessel damage run in `ConstructProjectileEngine`. Luanti map collision is currently supplied by the server Lua adapter through `core.raycast`, then returned to the native engine with `core.impact_dynamic_construct_projectile`.

## M4K navigation integration

M4K is a server-side controller and does not reserve another network opcode. `ConstructNavigationEngine` reads the authoritative construct registry and emits velocity/yaw commands that travel through the existing transform snapshot replication.

The NavyCraft Lua bridge configures movement limits from the craft type and current engine-derived top speed. Static terrain is observed through a small forward ray fan and submitted as short-lived observer-local obstacles. Other native constructs are considered directly in C++ with predicted closest-approach separation.

A later complete runtime pass must tune navigation against real server step jitter, unloaded map boundaries and large multiplayer fleets. The current controller performs local reactive avoidance, not a global terrain path search.

## M4L structural integration

M4L is server-side and uses the existing construct transform, section and remove packets. `ConstructStructureEngine` watches node revisions, runs ship-local connectivity analysis and creates new `DynamicConstruct` records for disconnected components.

When a split occurs, the server sends a remove packet for the surviving original ID and immediately resends its complete transform and sections. This clears stale client sections that belonged to detached pieces. Each new fragment then receives the ordinary full-construct replication sequence.

Projectile breach results are forwarded from the native projectile Lua step into the structural engine. The source-derived flooding system can also set a native flooded fraction. Fragment transforms are stepped by the structural engine and broadcast through the existing interpolation path.

Protocol capability version 9 identifies the M4L Lua API. No command numbers beyond M4I's `0x6A` projectile packet are reserved.

## M4M articulation integration

M4M reserves `TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION = 0x6B` and advances `TOCLIENT_NUM_MSG_TYPES` to `0x6C`. The packet stream carries reliable joint definitions/removals and low-latency state snapshots.

`ClientConstructScene` separates articulated nodes from ordinary section meshes, builds MapBlockMesh-backed joint meshes, and attaches them to a hierarchy of Irrlicht empty scene nodes below the vessel root. Joint snapshots are sampled using interpolation and bounded extrapolation before each frame.

The server sends articulation definitions and current state during `SendNavyCraftFullState`, so late joiners reconstruct the same hierarchy before subsequent state updates arrive. The Lua API uses protocol capability version 10.

The independent affine transform and obstruction code supports arbitrary joint axes. The current Irrlicht scene adapter is compatibility-compiled and practically validated for cardinal axes; a complete patched-client runtime pass remains required.

## M4N articulated physics and liquid integration

M4N does not reserve another client command. Articulated riders continue to use `TOSERVER_NAVYCRAFT_RIDER_STATE`; the versioned payload now carries an optional articulation ID and joint-local anchor. Old version-1 rider packets remain decodable.

`ClientConstructScene` rebuilds a lightweight collision registry from sampled client construct and articulation state before local-player movement. It applies joint-platform displacement, then resolves the player's AABB against arbitrary-axis articulated node boxes. Server reconciliation repeats support validation through the authoritative articulation engine.

Construct liquid cells are part of serialized `DynamicConstruct` data format version 3 and therefore travel through persistence and rollback records. Compartments, pumps and breaches are server runtime definitions managed by the Lua/game bridge. No extra visual packet is required for the authoritative liquid state in M4N; flooding effects use the existing construct-effect channel.

Protocol capability version 11 identifies the new Lua APIs. A complete patched-client/server compile and live runtime pass remains required.
