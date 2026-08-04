# Native integration status — Gate 9

## 2026-08-04 Codex hardening pass

- Promoted the handoff source to the workspace root and cloned the pinned Luanti 5.16.1 commit locally with `core.autocrlf=false`.
- `verify-upstream.py` now verifies pinned Git object IDs instead of hashing Windows working-tree bytes, avoiding false CRLF failures.
- `apply-engine-overlay.py` now patches Luanti `main.cpp` with compiled `--navycraft-version` and `--navycraft-protocol` probes.
- `verify-native-binaries.ps1` now searches Luanti source and CMake build output locations, then requires both `luanti.exe` and `luantiserver.exe` to report NavyCraft version `0.9.0` and protocol `16`.
- Added `scripts/run-native-startup-acceptance.ps1`, which starts the built dedicated server against the NavyCraft game and requires the native protocol activation log line while rejecting removed Lua renderer markers.
- `scripts/test-overlay.py` now verifies a real patched Luanti tree when a path is supplied, not only the synthetic fixture.
- The prototype CMake harness can now run non-database native regression tests without SQLite installed; SQLite-backed persistence coverage is still enabled automatically when SQLite is available.
- `BUILD_NATIVE_WINDOWS.ps1` now locates Visual Studio's bundled CMake/CTest when they are not on `PATH`.
- Local MSVC prototype build, syntax objects, regression test and benchmark pass in this workspace without installing dependencies.

## Permanently implemented

- Exact upstream base: Luanti 5.16.1 commit `5ebd9b57984d5854e0a37fd0125da48e9e59e190`.
- Native construct common, client and server sources are integrated into Luanti CMake source lists.
- Authoritative server transforms use a deterministic 60 Hz fixed step with bounded overload handling.
- Native transform replication runs at 20 Hz and client interpolation uses a synchronized network clock.
- Native construct protocol capability is **16**.
- Native client/server capability negotiation is mandatory before any construct state, rider state or interaction traffic is accepted.
- Protocol negotiation validates the exact protocol version plus moving collision, rider authority, interaction, fixed-step simulation and clock-synchronised snapshot features.
- Moving-platform displacement and moving-hull collision are integrated into `LocalPlayer::move`.
- Native finalization runs after Luanti sneak/ledge correction and immediately before the final `setPosition`, so rider packets and local pose cannot diverge within the frame.
- A deterministic swept broad-phase rejects distant constructs using current and predicted bounds.
- Linear and yaw motion are included conservatively in predicted collision bounds.
- An existing supporting construct is always retained by the broad-phase query.
- Support-node hysteresis prevents oscillation across neighbouring deck blocks.
- Rider packets include validated construct-local velocity.
- The server carries players by actual platform-anchor displacement before bounded predictive reconciliation.
- Exact server position overwrite on every rider tick has been removed.
- Lua construct rendering, hull collision entities, rider teleporting and manual construct stepping have been physically removed.
- Engine-independent C++ builds, integration syntax targets, overlay idempotence, regression tests, broad-phase benchmark and Lua smoke tests pass.

## Hard gate still open

The complete patched Luanti 5.16.1 Windows client and server must compile and link against the exact upstream tree. The native path must then pass real process startup, local deck collision, turning, jumping, deck exit and at least two-client rider synchronization tests before release.

## No user test yet

Gate 9 is a permanent source checkpoint, not an installed native base. The first meaningful user test begins only after the package contains compiled `luanti.exe` and `luantiserver.exe` files exposing protocol 16 and those binaries have passed automated runtime acceptance.
