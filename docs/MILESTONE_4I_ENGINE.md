# Milestone 4I — server-authoritative projectiles and construct damage

Milestone 4I moves NavyCraft weapon flight and vessel damage out of client-visible Lua entities and into the native server construct runtime. The supplied NavyCraft weapon catalogue remains the gameplay source for weapon names, salvo counts, ammunition and explosive strengths. Projectile speed, range and continuous collision are Luanti adaptations because Bukkit block-step motion does not map directly to a smooth moving-construct engine.

## Projectile runtime

`ConstructProjectileEngine` owns every active projectile. Supported kinds are:

- shell;
- fireball;
- torpedo;
- depth charge;
- bomb;
- anti-aircraft round.

Each projectile records its source and optional target construct, owner, previous and current world position, velocity, radius, arming time, age, travelled distance, range, blast radius, blast power, penetration and guidance settings.

The game adapter advances native projectiles in fixed steps of at most 0.05 seconds. Gravity and drag are applied by the server. Guided torpedoes turn toward the target construct and correct toward their selected running depth.

## Moving-vessel collision

Projectiles are not tested only at their final position. The server sweeps their full movement segment against each construct's current rotated voxel geometry.

For projectiles with non-zero radius, M4I sweeps a projectile-sized AABB through the segment. This catches grazing impacts that a centre-line ray would miss. The nearest construct or map collision wins.

Construct hits are calculated against ship-local blocks after transforming the projectile into the vessel's current frame. A vessel can therefore translate and rotate without converting itself back into static world nodes.

## Explosion and armour model

An explosion evaluates construct nodes within its blast radius. Effective power decreases with distance. A direct hit receives the projectile's penetration contribution.

The native fallback armour resolver uses node names when no engine-specific resolver is installed:

- armour or obsidian: 8.0;
- steel or iron: 4.0;
- wood: 0.8;
- glass: 0.5;
- other nodes: 1.5.

Destroyed nodes are removed from the native construct. Torpedo, depth-charge and low-waterline destruction can create breaches. The gameplay mirror updates hull integrity, flooding, helm state, last attacker and damage hooks. Blast impulse changes the struck construct's linear velocity according to remaining mass.

This armour table is an M4I engine adaptation, not a value table recovered from the supplied Bukkit source.

## World and entity damage

The C++ runtime owns construct collision and construct damage. Ordinary Luanti map nodes, players and Lua entities remain part of the server environment. The Lua bridge therefore:

1. raycasts each authoritative projectile segment against the Luanti map;
2. reports the first terrain or water impact back to the native engine;
3. applies protected terrain damage from the returned authoritative explosion;
4. applies distance-falloff damage to nearby players and punchable Lua entities.

Torpedoes and depth charges pass through liquid nodes and stop on solid terrain. Torpedoes that leave water after arming are removed with a water-impact event.

## Projectile protocol

`TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE` uses command `0x6A`. The client command range ends at `0x6B`.

Packets are versioned and contain spawn, update, impact or remove state. They include projectile kind, IDs, position, previous position, velocity, age, range state and blast parameters. Impact packets contain the collision type, point, normal, struck construct/local node and aggregate destruction counts.

The codec rejects malformed, oversized, non-finite and stale data. Update packets are sent unreliably; spawn, impact and remove packets are reliable.

## Client presentation

The M4I client state manager reconstructs authoritative projectile positions and rejects stale sequence numbers. It currently presents flight using particle trails and impacts using existing M4H positional sounds and particle effects.

A dedicated projectile mesh/model scene node is not yet implemented. The projectile simulation and hits do not depend on the visual particle arriving.

## Lua API

```lua
core.spawn_dynamic_construct_projectile(source_id, definition)
core.step_dynamic_construct_projectiles(delta_seconds)
core.get_dynamic_construct_projectiles()
core.impact_dynamic_construct_projectile(projectile_id, world_position, kind)
```

The game uses these APIs automatically when protocol version 6 is available. Stock Luanti retains the prior Lua projectile entity as a development fallback.

## Verification completed

- shell impact against a 45-degree rotated construct;
- non-zero-radius grazing collision that a centre ray misses;
- explosion falloff and native node removal;
- packet encode/decode and stale-sequence rejection;
- guided torpedo steering and depth correction;
- native Lua fire path and construct damage mirroring;
- nearby-player blast damage;
- client and server compatibility compilation;
- release, AddressSanitizer and UndefinedBehaviorSanitizer builds;
- repeated overlay application;
- gameplay regression smoke test.

## Runtime boundary

Not yet completed:

- linking the complete modified Luanti 5.16.1 executable;
- real client/server packet and latency testing;
- dedicated projectile meshes or tracers;
- direct C++ map raycast wiring instead of the Lua server bridge;
- damage to built-in non-Lua engine objects beyond the exposed ObjectRef interface;
- large-battle projectile load measurement.
