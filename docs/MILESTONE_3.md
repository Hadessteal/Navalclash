# Milestone 3 — complete source-parity gameplay pass

Milestone 3 changes the project from a movement demonstration into a broad NavyCraft game prototype based on the supplied NavyCraft and Shipyard binaries.

## New game modules

- `weapons.lua`: cannon, torpedo, depth-charge, bomb, AA and damage systems.
- `storage.lua`: store, select, claim, recall, spawn and repair.
- `routes.lua`: named routes, waypoints, autotravel and automatic craft spawning.
- `radio.lua`: four-channel radio and crew chat.
- `nc_shipyard`: source command surface, native plots, rewards, ranks and paydays.

## New native engine modules

- `construct_section`: 16³ local voxel sections for meshing, collision and delta replication.
- `construct_replication`: server snapshot interpolation and capped prediction.

## Verification

- Every Lua source file passes `texluac -p`.
- Full mocked game load passes.
- Launch, firing, storage and respawn smoke flow passes.
- Native C++ Release build passes with warnings treated as errors.
- Native unit tests pass.
- Store-neutral platform adapter builds.
- Engine overlay application is idempotent in the Luanti 5.16.1 fixture.
