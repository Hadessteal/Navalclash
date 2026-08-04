# Milestone 4E — construct-local interaction and state

Milestone 4E routes clicks against native moving constructs into a server-authoritative construct-local interaction layer. It also adds persistent per-node metadata, inventory lists and node timers without pretending moving nodes occupy ordinary world-map positions.

## Implemented

### Construct pointing and interaction packets

- The client compares the nearest native construct ray hit with Luanti's ordinary `PointedThing` target.
- A construct only receives the action when its hit is closer than the selected world node or active object.
- Versioned, bounded and reliable client-to-server interaction packets carry:
  - monotonic sequence;
  - construct ID;
  - action;
  - target and adjacent ship-local positions;
  - local/world hit point and surface normal;
  - wielded item string;
  - actor, form name, fields and client timestamp.
- The server rejects stale sequences, missing constructs, distant hits and hits that fail an authoritative construct raycast.

### Interaction actions

The native event layer represents start-dig, stop-dig, completed dig, place/right-click, use, activate, receive-fields and timer actions.

- Completed digging removes the ship-local node and replicates its dirty section.
- Place/right-click first becomes a Lua callback event so physical controls can consume the action.
- When no control consumes the action, the game adapter performs default construct-local node placement if the adjacent cell is empty.
- Placement and removal are mirrored into the Lua gameplay construct so the source-derived NavyCraft systems continue to see the same hull.

### Per-node state

Each construct node may now own:

- string metadata fields;
- named inventory lists with width and serialized item-stack strings;
- one persistent timer;
- a monotonic state revision.

Removing a construct node removes its state. The construct serialization format is now version 2 and retains metadata, inventory lists, timer state and revisions while remaining able to read version 1 payloads.

### Timer lifecycle

- Timers use ship-local node positions.
- Timers are stepped once in the server engine loop, independently of Lua-driven vessel movement.
- Expired timers queue one callback event and remain stopped until Lua resolves the event.
- A callback may stop the timer, restart its original timeout or return a positive replacement timeout.

### Lua callback adapter

`nc_core/dynamic_interactions.lua` exposes construct-local context objects with:

- `context.meta` string/int/float access;
- `context.inventory` list and stack access;
- `context.timer` start/stop/status access;
- player, construct ID, local positions, hit data and fields;
- access to the corresponding source-derived Lua gameplay construct.

Moving node definitions use explicit callback names such as:

- `_navycraft_on_punch`
- `_navycraft_on_dig`
- `_navycraft_on_rightclick`
- `_navycraft_on_use`
- `_navycraft_on_receive_fields`
- `_navycraft_on_timer`

The NavyCraft gameplay layer also registers a global adapter so its existing helm, engine, rudder, sonar, radar, weapon, pump, radio and other physical-control behavior works when the clicked node is inside a native moving vessel.

## Why callbacks are explicit

Ordinary Luanti node callbacks receive integer world-map positions and operate through world metadata and inventories. A smoothly translated or rotated moving node has no stable ordinary map position. M4E therefore does not silently call standard `on_dig`, `on_rightclick` or `on_timer` with a fabricated world coordinate. It uses an explicit construct-local callback contract instead.

## Verification

- Release build with warnings treated as errors.
- Unit tests for metadata revisions, inventories, timer expiry/restart, dig events, place/right-click events, packet round trips and malformed packets.
- Serialization version 2 round trip for node metadata, inventories and active timers.
- A compiler-order bug that could swap decoded metadata keys and values was found and fixed in both packet and persistence decoders.
- Client and server integration compatibility builds.
- Overlay fixture applied twice without duplicate changes.
- Lua syntax pass and game smoke test, including metadata, inventory and timer callback wrappers.
- AddressSanitizer and UndefinedBehaviorSanitizer verification.

## Remaining boundary

- The complete modified Luanti client and server executable still requires a real upstream-source build and runtime pass.
- Default placement does not yet consume items or calculate the complete stock placement orientation/rules.
- Digging time, tool wear, drops, protection callbacks and rollback logging are not yet fully virtualized.
- Standard formspec submission is represented in the core request format, but construct-local formspec routing is not yet connected to Luanti's GUI packet path.
- Node metadata and inventory state have a versioned construct persistence format, but the native registry is not yet connected to a production world database.

The next engine slice is M4F: complete digging/placement semantics, tool wear and drops, construct inventories/formspecs, protection/rollback integration and database-backed native construct persistence.
