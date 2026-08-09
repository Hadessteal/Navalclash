#!/usr/bin/env python3
"""Generate NavyCraft block registrations and texture mappings from a block ID table."""

from __future__ import annotations

import argparse
import html
import json
import re
import urllib.request
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE_URL = "https://minecraft-ids.grahamedgecombe.com/"
DEFAULT_DATA_PATH = REPO_ROOT / "data" / "navycraft_block_map.json"
DEFAULT_LUA_PATH = REPO_ROOT / "game" / "navycraft" / "mods" / "nc_blocks" / "block_registry.lua"
COLORS = [
    "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
    "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black",
]
WOOD_TYPES = ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak"]
STONE_VARIANTS = ["stone", "granite", "polished_granite", "diorite", "polished_diorite", "andesite", "polished_andesite"]


def slug(value: str) -> str:
    value = html.unescape(value).lower().replace("'", "")
    value = value.replace("+", " plus ")
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return re.sub(r"_+", "_", value).strip("_") or "block"


def parse_rows(page: str) -> list[dict[str, str]]:
    pattern = re.compile(
        r'<tr class="row">\s*'
        r'<td class="id">([^<]+)</td>.*?'
        r'<span class="name">([^<]+)</span><br /><span class="text-id">\(([^)]+)\)</span>',
        re.S,
    )
    rows = []
    for legacy_id, name, source_id in pattern.findall(page):
        base_id = int(legacy_id.split(":", 1)[0])
        if base_id > 255:
            continue
        display_name = html.unescape(name)
        navy_id = slug(display_name)
        if source_id.endswith(":air"):
            continue
        rows.append({
            "legacy_id": legacy_id,
            "display_name": display_name,
            "source_id": source_id,
            "navy_id": navy_id,
            "node_name": f"nc_blocks:{navy_id}",
            "texture": f"nc_block_{navy_id}.png",
            "source_textures": texture_candidates(display_name, source_id, legacy_id),
        })
    return disambiguate(rows)


def texture_candidates(display_name: str, source_id: str, legacy_id: str) -> list[str]:
    source_path = source_id.split(":", 1)[1]
    name_slug = slug(display_name)
    candidates = [
        f"minecraft:block/{name_slug}",
        f"minecraft:blocks/{name_slug}",
        f"block/{name_slug}",
        f"blocks/{name_slug}",
    ]

    candidates.extend(expanded_legacy_candidates(source_path, name_slug, legacy_id))
    candidates.extend([
        f"minecraft:block/{source_path}",
        f"minecraft:blocks/{source_path}",
        f"block/{source_path}",
        f"blocks/{source_path}",
        source_path,
        name_slug,
    ])
    seen = set()
    result = []
    for candidate in candidates:
        if candidate not in seen:
            seen.add(candidate)
            result.append(candidate)
    return result


def expanded_legacy_candidates(source_path: str, name_slug: str, legacy_id: str) -> list[str]:
    meta = int(legacy_id.split(":", 1)[1]) if ":" in legacy_id else 0
    result: list[str] = []
    if source_path == "stone" and meta < len(STONE_VARIANTS):
        result.append(f"minecraft:block/{STONE_VARIANTS[meta]}")
    if source_path in {"planks", "sapling", "log", "leaves", "log2", "leaves2"}:
        wood = WOOD_TYPES[meta % len(WOOD_TYPES)]
        if source_path == "planks":
            result.append(f"minecraft:block/{wood}_planks")
        elif source_path == "sapling":
            result.append(f"minecraft:block/{wood}_sapling")
        elif source_path in {"log", "log2"}:
            result.extend([f"minecraft:block/{wood}_log", f"minecraft:block/{wood}_log_side"])
        elif source_path in {"leaves", "leaves2"}:
            result.append(f"minecraft:block/{wood}_leaves")
    if source_path in {"wool", "stained_glass", "stained_glass_pane", "stained_hardened_clay", "concrete", "concrete_powder", "carpet"}:
        color = COLORS[meta % len(COLORS)]
        modern = {
            "wool": f"{color}_wool",
            "stained_glass": f"{color}_stained_glass",
            "stained_glass_pane": f"{color}_stained_glass_pane_top",
            "stained_hardened_clay": f"{color}_terracotta",
            "concrete": f"{color}_concrete",
            "concrete_powder": f"{color}_concrete_powder",
            "carpet": f"{color}_wool",
        }
        result.append(f"minecraft:block/{modern[source_path]}")
    if source_path == "sand" and meta == 1:
        result.append("minecraft:block/red_sand")
    if source_path == "sandstone":
        result.extend(["minecraft:block/sandstone", "minecraft:block/chiseled_sandstone", "minecraft:block/cut_sandstone"])
    if source_path == "red_sandstone":
        result.extend(["minecraft:block/red_sandstone", "minecraft:block/chiseled_red_sandstone", "minecraft:block/cut_red_sandstone"])
    if "door" in source_path:
        result.extend([f"minecraft:block/{source_path}_top", f"minecraft:block/{source_path}_bottom"])
    if source_path.endswith("_stairs"):
        result.append(f"minecraft:block/{source_path.removesuffix('_stairs')}")
    if source_path.endswith("_slab"):
        result.append(f"minecraft:block/{source_path.removesuffix('_slab')}")
    if name_slug.endswith("_slab"):
        result.append(f"minecraft:block/{name_slug.removesuffix('_slab')}")
    if name_slug.endswith("_stairs"):
        result.append(f"minecraft:block/{name_slug.removesuffix('_stairs')}")
    return result


def disambiguate(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    counts: dict[str, int] = {}
    for row in rows:
        navy_id = row["navy_id"]
        counts[navy_id] = counts.get(navy_id, 0) + 1
        if counts[navy_id] > 1:
            suffix = row["legacy_id"].replace(":", "_")
            row["navy_id"] = f"{navy_id}_{suffix}"
            row["node_name"] = f"nc_blocks:{row['navy_id']}"
            row["texture"] = f"nc_block_{row['navy_id']}.png"
    return rows


def lua_quote(value: str) -> str:
    return json.dumps(value)


def groups_for(row: dict[str, str]) -> dict[str, int]:
    name = row["navy_id"]
    groups = {"oddly_breakable_by_hand": 1}
    if any(word in name for word in ["leaves", "sapling", "flower", "mushroom", "grass", "fern", "vine", "carpet"]):
        groups = {"snappy": 2, "flammable": 2}
    elif any(word in name for word in ["wood", "plank", "log", "fence", "door", "chest", "bookshelf", "ladder"]):
        groups = {"choppy": 2, "flammable": 2}
    elif any(word in name for word in ["sand", "gravel", "dirt", "clay", "snow", "powder"]):
        groups = {"crumbly": 2}
    elif any(word in name for word in ["ore", "stone", "brick", "concrete", "terracotta", "obsidian", "block", "furnace", "anvil"]):
        groups = {"cracky": 2}
    if any(word in name for word in ["hull", "wood", "plank", "log", "stone", "brick", "concrete", "glass", "iron", "gold", "diamond"]):
        groups["navycraft_hull"] = 1
        groups["navycraft_weight"] = 60 if "iron" in name or "gold" in name else 30
    return groups


def write_lua(rows: list[dict[str, str]], path: Path) -> None:
    lines = [
        "-- Generated by tools/pack_converter/generate_block_data.py.",
        "-- Source-derived identifiers are kept in data/navycraft_block_map.json; game nodes use nc_blocks names.",
        "return {",
    ]
    for row in rows:
        groups = groups_for(row)
        group_text = ",".join(f"{key}={value}" for key, value in sorted(groups.items()))
        lines.append(
            "  {"
            f"id={lua_quote(row['navy_id'])},"
            f"name={lua_quote(row['display_name'])},"
            f"texture={lua_quote(row['texture'])},"
            f"groups={{{group_text}}}"
            "},"
        )
    lines.append("}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-url", default=DEFAULT_SOURCE_URL)
    parser.add_argument("--html-file", type=Path)
    parser.add_argument("--data", type=Path, default=DEFAULT_DATA_PATH)
    parser.add_argument("--lua", type=Path, default=DEFAULT_LUA_PATH)
    args = parser.parse_args()

    if args.html_file:
        page = args.html_file.read_text(encoding="utf-8")
    else:
        request = urllib.request.Request(args.source_url, headers={"User-Agent": "NavyCraftConverter/1.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            page = response.read().decode("utf-8")

    rows = parse_rows(page)
    args.data.parent.mkdir(parents=True, exist_ok=True)
    args.data.write_text(json.dumps({
        "source_url": args.source_url,
        "license_note": "Names and legacy IDs are factual compatibility data. Textures must come from open-licensed packs.",
        "node_namespace": "nc_blocks",
        "texture_prefix": "nc_block_",
        "blocks": rows,
    }, indent=2), encoding="utf-8")
    write_lua(rows, args.lua)
    print(f"Generated {len(rows)} block mappings")
    print(f"Wrote {args.data}")
    print(f"Wrote {args.lua}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
