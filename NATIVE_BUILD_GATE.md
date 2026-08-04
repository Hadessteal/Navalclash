# Native engine build gate — Gate 9

The temporary Lua moving-ship renderer no longer exists. A vessel may launch only when the patched Luanti executable exposes NavyCraft construct protocol version **16**.

Gate 9 permanently corrects the player-motion ordering: native platform displacement occurs before stock physics, while native collision finalization and rider-state generation occur after stock sneak/ledge correction and directly before Luanti commits the final player position. Protocol and feature negotiation remain mandatory before any native construct, collision, rider or interaction traffic is accepted. The already-native `LocalPlayer::move` collision and rider-authority pipeline remains the only ship path. Compiler, linker or runtime failures are unfinished native-engine work and must be corrected in C++; they are never hidden behind Lua movement or rider-position workarounds.

The release gate remains closed until the exact Luanti 5.16.1 client and server compile, native collision works in runtime, correction counts remain bounded, and multiplayer rider synchronization passes.
