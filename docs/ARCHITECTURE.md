# Architecture

## Product layout

```text
NavyCraft launcher / storefront
        |
        v
NavyCraft game executable (modified Luanti)
        |
        +-- LGPL engine fork
        |     +-- dynamic construct storage
        |     +-- rendering and interpolation
        |     +-- moving-platform physics
        |     +-- construct-aware collision/raycast
        |     +-- construct networking
        |
        +-- original game layer
        |     +-- hull detection and ship types
        |     +-- buoyancy, flooding and pumps
        |     +-- engines, sails and fuel
        |     +-- weapons, sonar and crew
        |
        +-- optional platform adapter
              +-- Null/offline adapter
              +-- Steam adapter (later)
              +-- Epic Online Services adapter (later)
              +-- GOG Galaxy adapter (later)
```

## Core rule: constructs are not entities made from thousands of blocks

A launched craft becomes a dynamic voxel construct with its own local coordinate system. Rendering is chunked and meshed. A single transform moves the construct; individual blocks remain at integer local coordinates.

## Milestone-1 constraints

The first playable dynamic construct will support:

- smooth translation;
- smooth yaw rotation only;
- level decks with no pitch or roll;
- world collision;
- players walking and jumping on deck;
- local digging, placement and inventory interaction;
- server-authoritative multiplayer interpolation;
- launch and dock conversion;
- a 5,000-node default craft limit.

Pitch, roll, waves and aircraft banking are deferred until the yaw-only physics is stable.

## Storefront separation

The engine must not include private storefront SDK headers. The game talks to a small C ABI. A store-specific dynamic library may implement that ABI. The default null implementation provides offline operation and direct-IP multiplayer.

This gives every storefront the same game build semantics while keeping achievements, cloud saves, invites and ownership checks optional.
