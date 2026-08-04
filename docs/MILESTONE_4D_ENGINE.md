# Milestone 4D — moving-hull collision and carried objects

Milestone 4D extends the M4C walkable-deck path into a reusable moving-hull collision and server object-carrier layer.

## Implemented

- Detailed yaw-aware voxel overlap queries with penetration depth and outward contact normals.
- Detailed swept AABB hits with construct ID, local node, contact normal, contact point and surface velocity.
- Iterative collision response that slides along rotated hull walls.
- Separate floor, wall and ceiling contact classification.
- Initial-overlap depenetration for a hull that moves into an object.
- Relative-motion handling using construct linear and angular surface velocity.
- Local-player side, head and ceiling collision pass after normal Luanti movement and deck inheritance.
- Platform velocity inheritance when a rider jumps or naturally leaves a deck.
- Persistent carrier state for remote players, creatures and dropped items.
- Server-side remote-rider transform updates from validated ship-local anchors.
- Server active-object collection around construct bounds.
- Native movement inheritance and hull collision response for unattached Lua entities.
- Special dropped-item classification for `__builtin:item`.
- Server lifecycle hook after the ordinary environment step.

## Verification

The independent engine core is built with warnings as errors. Tests cover wall stopping, floor contacts, ceiling contacts, rotated overlap normals, dropped-item deck acquisition and carried movement. Release tests, AddressSanitizer, UndefinedBehaviorSanitizer, the server integration compatibility build, overlay idempotency, platform adapter and Lua game smoke tests pass.

## Remaining limitation

The complete patched Luanti 5.16.1 executable has not been compiled or run in this environment. The overlay is checked against source anchors and compatibility stubs, but a real client/server build may require small include or engine-lifecycle adjustments.

The next engine slice is moving interaction: merge construct ray hits into `PointedThing`, transmit construct-local dig/place actions and run node callbacks, inventories and timers against moving storage.
