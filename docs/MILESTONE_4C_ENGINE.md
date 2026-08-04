# Milestone 4C — native rider contacts and reconciliation

This milestone extends the Luanti 5.16.1 engine overlay with the first native walkable-deck path. It does not alter the NavyCraft and Shipyard gameplay rules derived from the supplied JARs.

## Implemented

### Deck support queries

- Finds a supporting construct node beneath a player AABB.
- Works with smooth translation and unrestricted yaw because yaw does not tilt deck tops.
- Uses a bounded drop/penetration window and yaw-aware horizontal overlap.
- Selects the closest valid support surface.

### Persistent rider contacts

- Stores a construct ID and ship-local foot anchor.
- Applies vessel translation and rotational displacement each client frame.
- Re-anchors after ordinary player walking so movement remains relative to the deck.
- Includes a short contact grace window for packet jitter and block-edge transitions.
- Clears safely when the construct disappears.

### Jump and landing behavior

- Snaps a descending player onto a nearby deck top.
- Marks the local player grounded while supported.
- Detects the jump button edge.
- Detaches the rider on jump.
- Adds the deck's measured or commanded surface velocity to jump velocity.

### Rider protocol

A versioned client-to-server rider packet is added at `0x54`. It contains:

- monotonic sequence;
- construct ID or zero for detached;
- ship-local anchor;
- reported world position and velocity;
- client time;
- grounded and jumping flags.

Packets are bounded to 256 bytes, reject non-finite values, reject trailing/truncated data and use unreliable channel 0 like ordinary player-position updates.

### Server validation and reconciliation

- Rejects stale rider sequences.
- Validates the construct and reported local anchor.
- Verifies that a grounded packet still has a supporting construct node.
- Expires rider state after one second without updates.
- Soft-corrects moderate drift and hard-snaps excessive drift.
- Uses Luanti's existing `SendMovePlayer` correction for rejected rider states.
- Exempts validated riders from the ordinary moved-too-fast check while aboard.

### Late-joining clients

When a client finishes joining, the server sends current transforms and all construct sections before normal live updates continue. Sequence allocation is shared with live replication, so the late-join snapshot cannot make later packets appear stale.

### Integration order

The client loop now performs:

1. sample and render native construct transforms;
2. run normal Luanti environment/player movement;
3. apply NavyCraft deck displacement and jump inheritance;
4. send rider state when contact changes or every 100 ms.

## Verification

- Release C++17 build with warnings treated as errors.
- Core unit tests for support detection, landing, carried movement, jump inheritance, packet validation and soft/hard reconciliation.
- AddressSanitizer and UndefinedBehaviorSanitizer pass.
- Native Lua API compatibility-stub compile.
- Client packet-integration compatibility-stub compile.
- Server rider bridge compatibility-stub compile.
- Luanti-shaped overlay fixture applies twice without duplicate edits.
- Lua gameplay smoke test passes under TeX Lua.

## Not yet complete

- The complete modified Luanti client/server executable has not been compiled in this environment.
- Side, ceiling and step-up collisions against moving constructs are not yet merged into `collisionMoveSimple`.
- Remote players, creatures and dropped items do not yet use the native rider solver.
- Full node materials, lighting and specialised drawtypes remain pending.
- Actual multiplayer runtime behavior still requires testing with two modified clients.

## Next engine slice

Milestone 4D should merge construct collision into player movement, add remote-entity platform inheritance, and combine ordinary world raycasts with construct-local dig/place/use interactions.
