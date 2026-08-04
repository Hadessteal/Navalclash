HISTORICAL RECORD — superseded by Gate 4.

# Gate 3 — permanent authoritative motion and clock synchronization

Gate 3 is a permanent engine implementation checkpoint. It is not a user test build.

## Engine changes

- `ConstructSimulation` advances every native construct at a deterministic 60 Hz.
- Frame input is bounded and substep overload is dropped deterministically.
- The server emits authoritative transform snapshots at 20 Hz.
- `ConstructNetworkClock` maps client render time to the server snapshot clock while rejecting queueing-delay spikes.
- Client scene interpolation samples the synchronized server timeline.
- Rider platform displacement is applied before Luanti's normal local-player physics step.
- The old post-physics rider correction ordering has been removed.
- The manual Lua registry-step API has been removed, preventing accidental double simulation.

## Removed code

- Lua construct block entities.
- Lua hull rendering.
- Lua passenger capture and teleporting.
- Lua moving-deck fall-damage suppression.
- Restoration of `lua-*` vessels.
- `preview_constructs.lua`.

`native_construct_state.lua` remains as the gameplay metadata/control adapter. It reads authoritative native state and sends control values; it does not integrate transforms or move players.

## Verification completed

- All native common sources compile with warnings treated as errors.
- Client scene and integration syntax targets compile.
- Server replication, simulation, rider and interaction syntax targets compile.
- Fixed-step equivalence, overload bounds and network-clock tests pass.
- Overlay application is idempotent.
- All Lua files parse.
- The complete Lua gameplay smoke suite passes with protocol 12.

## Remaining before user testing

- Compile and link the full patched Luanti 5.16.1 Windows client and server.
- Fix real upstream compile/link errors revealed by that build.
- Integrate native moving hull collision into the actual LocalPlayer collision sweep rather than relying only on the pre-physics rider preparation hook.
- Pass a two-client synchronization run.
- Produce the versioned `NavyCraft-Native-Windows-Base.zip` installation.

Only after those items pass will a drag-and-drop user update be issued.
