#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORLDGEN = ROOT / "game" / "navycraft" / "mods" / "nc_blocks" / "worldgen.lua"
INIT = ROOT / "game" / "navycraft" / "mods" / "nc_blocks" / "init.lua"
CORE_INIT = ROOT / "game" / "navycraft" / "mods" / "nc_core" / "init.lua"
GAME_CONF = ROOT / "game" / "navycraft" / "minetest.conf"
BLOCK_MAP = ROOT / "data" / "navycraft_block_map.json"
BLOCK_TEXTURE_DIR = ROOT / "game" / "navycraft" / "mods" / "nc_blocks" / "textures"


def main() -> int:
    worldgen = WORLDGEN.read_text(encoding="utf-8")
    init = INIT.read_text(encoding="utf-8")
    core_init = CORE_INIT.read_text(encoding="utf-8")
    game_conf = GAME_CONF.read_text(encoding="utf-8")
    block_map = json.loads(BLOCK_MAP.read_text(encoding="utf-8"))
    block_ids = {block["navy_id"] for block in block_map["blocks"]}

    if 'dofile(modpath .. "/worldgen.lua")' not in init:
        raise AssertionError("nc_blocks does not load worldgen.lua")
    if "mg_name = singlenode" not in game_conf:
        raise AssertionError("game default mapgen is not singlenode")
    if "core.register_on_generated" not in worldgen:
        raise AssertionError("worldgen does not register map generation callback")
    if "VoxelArea:new" not in worldgen or "get_perlin_map" not in worldgen:
        raise AssertionError("worldgen is not using voxel/perlin terrain generation")
    if "local size2d = {x = maxp.x - minp.x + 1, y = maxp.z - minp.z + 1, z = 1}" not in worldgen:
        raise AssertionError("2D Perlin map size must include z = 1 for Luanti vector validation")
    if "local origin2d = {x = minp.x, y = minp.z, z = 0}" not in worldgen:
        raise AssertionError("2D Perlin map origin must include z = 0 for Luanti vector validation")
    if "mapgen_tree" in core_init or "mapgen_leaves" in core_init:
        raise AssertionError("mapgen aliases should live in nc_blocks after nc_blocks nodes are registered")

    referenced = set(re.findall(r'cid\("nc_blocks:([^"]+)"\)', worldgen))
    missing = sorted(referenced - block_ids)
    if missing:
        raise AssertionError(f"worldgen references missing nc_blocks nodes: {missing}")

    missing_textures = [
        block["texture"] for block in block_map["blocks"]
        if not (BLOCK_TEXTURE_DIR / block["texture"]).is_file()
    ]
    if missing_textures:
        raise AssertionError(f"generated block textures are missing: {missing_textures[:10]}")

    required_aliases = {
        "mapgen_stone": "nc_blocks:stone",
        "mapgen_dirt": "nc_blocks:dirt",
        "mapgen_dirt_with_grass": "nc_blocks:grass",
        "mapgen_sand": "nc_blocks:sand",
        "mapgen_tree": "nc_blocks:oak_wood",
        "mapgen_leaves": "nc_blocks:oak_leaves",
    }
    for alias, target in required_aliases.items():
        expected = f'core.register_alias("{alias}", "{target}")'
        if expected not in worldgen:
            raise AssertionError(f"missing alias mapping: {expected}")

    if "minecraft" in worldgen.lower():
        raise AssertionError("worldgen should not use source namespace names")

    print(f"Worldgen test passed with {len(referenced)} nc_blocks terrain/material references")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
