# NavyCraft Native — Gate 9

This repository contains the complete NavyCraft gameplay project plus the Luanti 5.16.1 moving-construct engine overlay.

The planned backend feature families are complete through M4N. Gameplay is complete through M5G: careers, contracts, factions, fleets, industry, vessel progression, logistics, ownership, insurance, capture, salvage, tutorial onboarding, station interfaces, HUDs and accessibility.

## M5G additions

- persistent guided basic training;
- role-aware bridge HUD;
- full, minimal and disabled HUD modes;
- tabbed command, helm, engineering, weapons, sensors, mission and settings console;
- station-permission-aware controls;
- high-contrast and large-text presentation;
- colour-vision palettes;
- subtitle, reduced-motion and alert-filter preferences;
- deduplicated critical-alert queue and notification history;
- bridge console, training console and service handbook.

## First-session flow

Create a port and start a career:

```text
/port create home Home Harbour
/career start
/faction join navy
```

Open the integrated interface:

```text
/station
```

Follow the current training step:

```text
/tutorial status
/tutorial hint
```

At a registered port:

```text
/mission list
/mission accept sea_trial
```

## Presentation commands

```text
/station [command|helm|engineering|weapons|sensors|missions|settings]
/hud full|minimal|off
/accessibility status
/accessibility high_contrast on
/accessibility large_text on
/accessibility colorblind deuteranopia
/accessibility subtitles on
/accessibility reduced_motion on
/accessibility alerts critical
/notificationlog
```

## Backend status

The native moving-construct backend is now protocol capability **16**. Gate 9 moves native hull finalization to the true final-pose point inside `LocalPlayer::move`, after Luanti has completed stock collision, sneak-edge and ledge correction. The local position, native collision result and transmitted rider packet therefore describe the same frame-final pose.

The handshake requires native rendering, moving-hull collision, rider authority, interactions, fixed-step simulation, clock-synchronised snapshots and final-player-pose integration. A mismatched client or server is rejected before ship state is transmitted.

The client no longer tests the player against every vessel each frame. Current and predicted construct bounds select only relevant collision candidates, while an established support construct is always retained. The server carries riders by actual platform-anchor displacement and then applies bounded reconciliation toward the validated local rider path instead of overwriting the exact player position every tick.

This repository is still a source checkpoint. The outstanding engineering gate is the complete patched Luanti executable compile, followed by native deck collision and two-client rider synchronization runtime tests. No Lua renderer or rider-position workaround remains.

## Verification

```bash
./scripts/verify.sh
```

The suite builds the engine-independent core and integration fixtures, runs construct tests and benchmarks, applies the engine overlay twice, validates every Lua file, and runs the complete gameplay smoke flow through M5G.

## Native build gate update

The Lua entity renderer has been deleted from the game. Native constructs are the only ship implementation. Use `BUILD_NATIVE_WINDOWS.ps1` or the native Windows GitHub Actions workflow. A valid Gate 9 test executable must expose protocol 16, include both `luanti.exe` and `luantiserver.exe`, and launch `native-*` constructs only. Until those compiled binaries pass runtime acceptance, there is nothing useful for a player to test.
