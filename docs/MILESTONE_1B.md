# NavyCraft Milestone 1B

Milestone 1B is a prototype-fix release for the stock-Luanti visual preview.
It does not change the long-term engine plan: entity-per-block rendering is still temporary.

## Fixes

- `/nc_turn <degrees>` now queues a one-time turn and stops when the requested angle is reached.
- Added `/nc_spin <degrees_per_second>` for deliberate continuous turning.
- `/nc_stop` now clears forward speed, vertical speed, continuous spin, and queued turns.
- The preview now rotates around the craft centre instead of the helm/origin.
- Visual block positions are recalculated from the immutable original node snapshot every frame.
- Preview entities are visual-only so their temporary entity collision cannot shove the player or distort movement.
- Added a stronger passenger carrier that keeps nearby players in ship-local coordinates while the craft moves.
- Docking now detects duplicate destination cells before placing nodes.

## Known limitations

- The stock-Luanti prototype is still not true moving-platform physics.
- Walking on the deck is simulated by repositioning the player each step.
- Blocks at non-cardinal angles are a preview illusion; only cardinal docking is supported.
- Large craft must wait for native sectioned construct meshes.
