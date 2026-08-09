# NavyCraft Pack Converter

Standalone converter for open-licensed Minecraft-style texture packs.

It creates a Luanti texture pack folder with NavyCraft override names such as
`nc_frame.png`, `nc_helm_top.png`, and `nc_fire.png`. It does not edit the game
or engine.

The converter scans `game/navycraft` by default and guarantees that every
existing NavyCraft texture filename is present in the output. It uses matching
Minecraft-pack textures first. If a pack does not contain a good match for a
NavyCraft texture, it copies the current NavyCraft texture as a fallback so the
result is still loadable and usable.

It also loads `data/navycraft_block_map.json` by default. That file contains
the legacy source block IDs from the public ID list and the `nc_blocks:*` nodes
created for NavyCraft. The game node names do not use the source namespace.

## Usage

## Windows App

Double-click:

```text
tools\pack_converter\NavyCraft Pack Converter.cmd
```

The app auto-fills the output folder to the latest local build's texture-pack
directory, for example:

```text
_github-artifacts\31055169524\base-install-full\textures\navycraft_converted_pack
```

Choose a source pack `.zip` or folder, check the pack name, then press
`Convert`. Use `Dry Run` first if you want to see what it will do without
writing files.

## Command Line

```powershell
python tools\pack_converter\convert_pack.py `
  --input C:\Packs\SomeOpenPack.zip `
  --output C:\Packs\navycraft_someopenpack `
  --pack-name navycraft_someopenpack `
  --copy-unmapped `
  --overwrite `
  --require-license
```

Then copy or move the output folder into Luanti's texture-pack directory and
enable it from the client.

## Notes

- Only use packs whose license allows redistribution, modification, and use
  outside Minecraft.
- Keep `ATTRIBUTION.txt`, `conversion_report.json`, and the original license
  details with any redistributed converted pack.
- Minecraft JSON models are inventoried in `conversion_report.json`; they are
  not directly converted because Luanti meshes use different formats.
- Adjust `default_mapping.json` when a pack uses different source names.
- Use `--strict` when you want conversion to fail instead of falling back to
  current NavyCraft textures.

## Custom Blocks

Add extra conversion targets with `--custom-blocks`:

```json
{
  "blocks": [
    {
      "display_name": "Armour Plate",
      "node_name": "nc_blocks:armour_plate",
      "texture": "nc_block_armour_plate.png",
      "source_textures": ["custompack:block/armour_plate"]
    }
  ]
}
```

Then run:

```powershell
python tools\pack_converter\convert_pack.py `
  --input C:\Packs\SomeOpenPack.zip `
  --output C:\Packs\navycraft_someopenpack `
  --custom-blocks C:\Packs\custom_blocks.json
```
