# Gate 4 — Native LocalPlayer collision-pipeline integration

Gate 4 replaces the last speculative rider-movement ordering with a permanent integration inside Luanti's real local-player movement path.

## Why the previous order was wrong

Moving the rider before or after the entire client environment step leaves Luanti's map collision solver unaware of the moving hull during the player sweep. Correcting the position afterward produces visible snapping. Repeating an absolute player position from Lua produces even worse jitter and fall-damage accumulation.

## Permanent two-phase movement

`LocalPlayer::move` now calls the NavyCraft client scene in two phases:

1. `beginNavyCraftLocalPlayerMove`
   - samples the authoritative/interpolated construct transform;
   - applies displacement from an already-established deck contact;
   - does not acquire a new support contact;
   - stores the movement frame used for the matching finish call.

2. Luanti executes its ordinary `collisionMoveSimple` world sweep.

3. `finishNavyCraftLocalPlayerMove`
   - receives the actual post-map-collision position and velocity;
   - sweeps the player collision box against native moving hull geometry;
   - resolves articulated moving-part collision;
   - acquires or updates deck support only from the completed collision result;
   - respects a held jump and does not immediately recapture the player;
   - writes the final grounded state and sends the native rider packet.

This ordering makes moving constructs part of the same movement transaction as normal Luanti collision rather than a later correction.

## Protocol change

The construct protocol is now version 13. A protocol-13 client/server pair is required because the completed movement result and contact semantics differ from Gate 3.

## Removed paths

The implementation contains no Lua hull renderer, no Lua collision entity, no Lua player `set_pos`/`add_pos` carrying, and no manual Lua construct step.

## Verification scope

Gate 4 verification compiles every engine-independent construct source and the client/server integration syntax targets with warnings treated as errors. It also tests the overlay twice, exercises phased support acquisition and held-jump release, parses the game Lua, and runs the full gameplay smoke flow when `texlua` is available.

A complete upstream executable build and real runtime collision test are still required before user testing.
