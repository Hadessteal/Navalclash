# Native Windows package layout

Gate 5 maintains two permanent package types.

## Base installation

The versioned base contains:

- `bin/luanti.exe`;
- `bin/luantiserver.exe`;
- runtime DLLs;
- Luanti runtime data (`builtin`, `client`, `fonts`, `textures`, and locale data when present);
- `games/navycraft`;
- `native-engine-version.json`.

A player installs a base only when the ABI or package layout changes.

## Drag-and-drop update

The versioned update contains only the recompiled client/server binaries, required DLLs and engine-version metadata. The player closes NavyCraft and extracts it over a compatible base. World data and the installed game directory are not replaced by a binary-only update.

The update is accepted only when its construct protocol matches the base/game protocol. This is the distribution model that will be used after the first native base passes runtime testing.
