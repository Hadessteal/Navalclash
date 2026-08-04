# Licensing boundary

This is an engineering layout, not legal advice.

## Engine fork

Luanti is distributed under LGPL-2.1-or-later. Files copied into or modifying the Luanti engine must retain applicable notices and be distributed with corresponding source as required by that licence.

The initial files under `engine-overlay/` are marked SPDX `LGPL-2.1-or-later` to keep the boundary clear.

## Original game

The Lua game code and original media under `game/` are currently marked **All rights reserved** as a commercial-development default. The owner can later choose another licence.

Do not copy Mineclonia, VoxeLibre, Minecraft, Movecraft or NavyCraft third-party code/assets into this tree without an explicit licence audit.

## Store SDKs

Do not commit Steamworks, GOG Galaxy, Epic Online Services or other restricted SDK files. Store them in an ignored local directory or CI secret-backed dependency cache and compile them only into optional adapters.

## Release obligations checklist

Every public binary release should include:

- Luanti and third-party copyright notices;
- LGPL licence text;
- a durable link or bundled method for obtaining exact corresponding engine source;
- identification of engine modifications;
- an asset/code bill of materials;
- the store-specific EULA and privacy disclosures where applicable.
