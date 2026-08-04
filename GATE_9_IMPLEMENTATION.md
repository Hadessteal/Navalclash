# Gate 9 — Final player pose integration

Gate 9 corrects the native moving-platform ordering inside Luanti's real
`LocalPlayer::move` pipeline. Earlier gates finalized NavyCraft hull collision
immediately after `collisionMoveSimple`, before Luanti completed sneak-edge and
ledge corrections. That allowed stock movement to change the player after the
rider packet and native final position had already been produced.

Permanent changes:

- Native pre-physics platform displacement remains before stock collision.
- Native moving-hull finalization now runs after stock sneak/ledge correction.
- Rider packets are generated from the same final pose passed to `setPosition`.
- The patched source fixture verifies the exact ordering:
  pre-physics -> stock collision -> sneak correction -> native finalization ->
  final `setPosition`.
- Protocol 16 requires the `FinalPlayerPose` capability so older binaries cannot
  silently mix the old movement ordering with the corrected server.
- Engine version advances to 0.9.0.

This is permanent engine integration. No Lua renderer, entity deck, or rider
teleport fallback is introduced.
