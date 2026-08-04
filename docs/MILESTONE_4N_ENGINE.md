# Milestone 4N — articulated physics, construct liquids and specialised nodes

Milestone 4N closes the final planned specialised backend gaps before NavyCraft development shifts primarily to gameplay mechanics.

## Articulated physical contact

`ConstructArticulatedMotion` converts nodes owned by revolute or prismatic joints into arbitrary-axis oriented collision boxes. It provides:

- support detection in joint-local space;
- persistent articulation-local foot anchors;
- translation and rotation inheritance;
- surface velocity at the exact contact point;
- swept movement, depenetration and sliding against moving articulated nodes;
- jump and edge-detachment velocity inheritance.

The local client uses this path after ordinary Luanti movement. The rider packet now includes an optional articulation ID, and the server validates the corresponding joint-local anchor before reconciling a remote rider. The same carrier path is available to creatures and dropped items.

## Construct-local liquids

`DynamicConstruct` now persists liquid cells independently of solid nodes. Each liquid cell contains a liquid name, level from zero to eight and flags. Serialization format version 3 includes those cells, so SQLite vessel saves and rollback snapshots retain flooding state.

`ConstructLiquidEngine` adds explicit compartments and ports:

- sealed or unsealed compartments;
- optional liquid mixing;
- downward flow;
- horizontal equalisation;
- source, drain and breach ports;
- escaped-volume accounting;
- fill fraction and liquid centre-of-mass reporting.

Structural separation moves nearby liquid cells into the newly recentered fragment, preserving a flooded wreck section rather than resetting it to dry.

## Specialised moving nodes

`ConstructSpecialNodeEngine` supports definitions for:

- climbable nodes;
- breathable or non-breathable spaces;
- liquid-permeable nodes;
- attachable platforms;
- damage per second;
- construct-local conveyor velocity and acceleration.

The engine evaluates these definitions against static and articulated construct nodes and reports contacts, velocity changes, damage, breathability and immersed fraction. The NavyCraft Lua layer registers its pump, ballast and rudder components and exposes the API for later gameplay-specific specialised nodes.

## Lua API

Protocol capability version 11 adds:

- `core.configure_dynamic_construct_liquid_compartment`
- `core.configure_dynamic_construct_liquid_port`
- `core.set_dynamic_construct_liquid`
- `core.step_dynamic_construct_liquids`
- `core.get_dynamic_construct_liquids`
- `core.register_dynamic_construct_special_node`

NavyCraft configures a persistent bilge compartment, connects ballast flooding and pumps to native ports, and mirrors native fill levels into the existing flooding/buoyancy gameplay state.

## Deliberate boundary

M4N completes backend feature implementation, but it is not a claim that the final executable is production-ready. Still required before release are:

1. link and run the complete patched Luanti 5.16.1 client/server;
2. visually and physically test native constructs on real clients;
3. fix any upstream ABI or scene-integration issues exposed by that build;
4. conduct multiplayer soak and stress testing;
5. profile and optimise interest management, meshing, effects and physics.

Construct liquids currently have authoritative cell simulation, persistence and effects, not a dedicated animated liquid-surface mesh. Those presentation improvements can be developed as gameplay polish without changing the underlying persistence or compartment model.
