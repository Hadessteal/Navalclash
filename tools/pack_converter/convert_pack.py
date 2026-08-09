#!/usr/bin/env python3
"""Convert open-licensed Minecraft-style packs into Luanti/NavyCraft texture packs.

The converter does not embed anything into the game. It creates a standalone
texture-pack folder whose files can be dropped into Luanti's textures directory.
Minecraft JSON models are inventoried in the report because they are not a
drop-in Luanti mesh format.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".tga"}
MAPPED_IMAGE_EXTENSIONS = {".png"}
MODEL_EXTENSIONS = {".json", ".obj", ".mtl"}
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_GAME_DIR = REPO_ROOT / "game" / "navycraft"
DEFAULT_BLOCK_MAP = REPO_ROOT / "data" / "navycraft_block_map.json"
DEFAULT_FALLBACK_TEXTURE = DEFAULT_GAME_DIR / "mods" / "nc_core" / "textures" / "nc_frame.png"


@dataclass(frozen=True)
class PackFile:
    logical_name: str
    path: Path
    relative_path: str


def safe_name(value: str) -> str:
    value = value.replace("\\", "/").strip("/")
    value = re.sub(r"[^A-Za-z0-9_.\-/]+", "_", value)
    value = value.replace("/", "_")
    value = re.sub(r"_+", "_", value)
    return value.strip("._") or "converted"


def unpack_if_needed(source: Path) -> tuple[Path, tempfile.TemporaryDirectory[str] | None]:
    if source.is_dir():
        return source, None
    if not source.is_file() or source.suffix.lower() != ".zip":
        raise SystemExit(f"Input must be a directory or .zip file: {source}")

    temp = tempfile.TemporaryDirectory(prefix="navycraft-pack-")
    with zipfile.ZipFile(source) as archive:
        for member in archive.infolist():
            target = Path(temp.name, member.filename).resolve()
            root = Path(temp.name).resolve()
            if root not in target.parents and target != root:
                raise SystemExit(f"Refusing unsafe zip path: {member.filename}")
        archive.extractall(temp.name)
    return Path(temp.name), temp


def load_mapping(path: Path) -> dict[str, list[str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    targets = data.get("targets", data)
    if not isinstance(targets, dict):
        raise SystemExit(f"Invalid mapping file: {path}")
    result: dict[str, list[str]] = {}
    for target, candidates in targets.items():
        if isinstance(candidates, str):
            result[target] = [candidates]
        elif isinstance(candidates, list) and all(isinstance(item, str) for item in candidates):
            result[target] = candidates
        else:
            raise SystemExit(f"Invalid mapping candidates for {target}")
    return result


def merge_targets(targets: dict[str, list[str]], target: str, candidates: Iterable[str]) -> None:
    existing = targets.setdefault(target, [])
    seen = set(existing)
    for candidate in candidates:
        if candidate not in seen:
            seen.add(candidate)
            existing.append(candidate)


def load_block_targets(paths: Iterable[Path]) -> dict[str, list[str]]:
    targets: dict[str, list[str]] = {}
    for path in paths:
        data = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(data, dict) and isinstance(data.get("targets"), dict):
            for target, candidates in load_mapping(path).items():
                merge_targets(targets, target, candidates)
            continue
        blocks = data.get("blocks") if isinstance(data, dict) else data
        if not isinstance(blocks, list):
            raise SystemExit(f"Invalid block mapping file: {path}")
        for block in blocks:
            if not isinstance(block, dict):
                raise SystemExit(f"Invalid block entry in {path}")
            texture = block.get("texture")
            candidates = block.get("source_textures") or block.get("source_ids") or block.get("candidates")
            source_id = block.get("source_id")
            if isinstance(candidates, str):
                candidates = [candidates]
            if not texture or not isinstance(candidates, list):
                raise SystemExit(f"Block entry must include texture and source_textures: {block}")
            expanded = list(candidates)
            if isinstance(source_id, str) and ":" in source_id:
                source_path = source_id.split(":", 1)[1]
                expanded.extend([f"minecraft:block/{source_path}", f"minecraft:blocks/{source_path}", source_path])
            merge_targets(targets, texture, [str(candidate) for candidate in expanded])
    return targets


def asset_logical_name(path: Path, root: Path) -> str:
    rel_parts = path.relative_to(root).parts
    lowered = [part.lower() for part in rel_parts]
    if "assets" in lowered:
        assets_index = lowered.index("assets")
        if len(rel_parts) > assets_index + 4:
            namespace = rel_parts[assets_index + 1]
            if lowered[assets_index + 2] in {"textures", "models"}:
                tail = Path(*rel_parts[assets_index + 3:]).with_suffix("")
                return f"{namespace}:{tail.as_posix()}"
    return Path(*rel_parts).with_suffix("").as_posix()


def find_files(root: Path, extensions: set[str]) -> list[PackFile]:
    files: list[PackFile] = []
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in extensions:
            files.append(PackFile(asset_logical_name(path, root), path, path.relative_to(root).as_posix()))
    return sorted(files, key=lambda item: item.relative_path.lower())


def find_game_textures(game_dir: Path) -> dict[str, Path]:
    textures: dict[str, Path] = {}
    if not game_dir.is_dir():
        return textures
    for path in game_dir.rglob("*"):
        if path.is_file() and path.parent.name == "textures" and path.suffix.lower() in IMAGE_EXTENSIONS:
            textures.setdefault(path.name, path)
    return dict(sorted(textures.items()))


def index_images(images: Iterable[PackFile]) -> dict[str, PackFile]:
    index: dict[str, PackFile] = {}
    for image in images:
        keys = {
            image.logical_name.lower(),
            image.logical_name.split(":", 1)[-1].lower(),
            Path(image.logical_name.split(":", 1)[-1]).name.lower(),
        }
        for key in keys:
            index.setdefault(key, image)
    return index


def find_license_files(root: Path) -> list[Path]:
    names = {"license", "license.txt", "license.md", "copying", "copying.txt", "readme", "readme.txt", "readme.md"}
    matches = []
    for path in root.rglob("*"):
        if path.is_file() and path.name.lower() in names:
            matches.append(path)
    return sorted(matches)


def copy_file(source: Path, destination: Path, overwrite: bool, dry_run: bool) -> bool:
    if destination.exists() and not overwrite:
        return False
    if not dry_run:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    return True


def category_candidates(target: str) -> list[str]:
    name = target.lower()
    if "helm" in name:
        return ["oak_planks", "spruce_planks", "dark_oak_planks", "barrel_side", "barrel_top"]
    if "water" in name or "splash" in name or "bubble" in name:
        return ["water_still", "blue_stained_glass", "light_blue_stained_glass", "cyan_concrete"]
    if "smoke" in name:
        return ["smoke", "large_smoke", "gray_concrete_powder", "black_concrete_powder"]
    if "fire" in name:
        return ["fire_0", "fire_1", "flame", "glowstone"]
    if "spark" in name:
        return ["crit", "glowstone", "ochre_froglight", "yellow_concrete"]
    if "inventory" in name or "hud" in name or "panel" in name:
        return ["gray_concrete", "dark_oak_planks", "black_concrete", "iron_block"]
    if "player" in name:
        return ["white_concrete", "light_gray_concrete", "iron_block"]
    return ["iron_block", "light_gray_concrete", "stone", "polished_andesite", "smooth_stone"]


def find_png_match(candidates: Iterable[str], image_index: dict[str, PackFile]) -> PackFile | None:
    for candidate in candidates:
        lookup = candidate.lower().removesuffix(".png")
        match = image_index.get(lookup) or image_index.get(Path(lookup).name)
        if match and match.path.suffix.lower() in MAPPED_IMAGE_EXTENSIONS:
            return match
    return None


def convert(args: argparse.Namespace) -> int:
    source = args.input.resolve()
    output = args.output.resolve()
    mapping_path = args.mapping.resolve()
    mapping = load_mapping(mapping_path)
    block_map_paths = []
    if args.block_map:
        block_map_paths.append(args.block_map.resolve())
    block_map_paths.extend(path.resolve() for path in args.custom_blocks)
    block_targets = load_block_targets(block_map_paths)

    root, temp = unpack_if_needed(source)
    try:
        images = find_files(root, IMAGE_EXTENSIONS)
        models = find_files(root, MODEL_EXTENSIONS)
        image_index = index_images(images)
        game_textures = find_game_textures(args.game_dir.resolve()) if args.game_dir else {}
        all_targets = dict(mapping)
        for target, candidates in block_targets.items():
            merge_targets(all_targets, target, candidates)
        for texture_name in game_textures:
            all_targets.setdefault(texture_name, category_candidates(texture_name))
        license_files = find_license_files(root)

        if args.require_license and not license_files:
            raise SystemExit("No LICENSE/README/COPYING file found. Re-run without --require-license only if you verified the license another way.")

        copied: list[dict[str, str]] = []
        skipped: list[dict[str, str]] = []
        fallbacks: list[dict[str, str]] = []
        default_fallbacks: list[dict[str, str]] = []
        fallback_texture = args.fallback_texture.resolve() if args.fallback_texture else None

        for target, candidates in all_targets.items():
            match = find_png_match(candidates, image_index)
            destination = output / target
            if match:
                if copy_file(match.path, destination, args.overwrite, args.dry_run):
                    copied.append({"target": target, "source": match.relative_path})
                else:
                    skipped.append({"target": target, "reason": "exists", "source": match.relative_path})
            else:
                fallback = game_textures.get(target)
                if fallback:
                    if copy_file(fallback, destination, args.overwrite, args.dry_run):
                        copied.append({"target": target, "source": fallback.as_posix()})
                        fallbacks.append({"target": target, "source": fallback.as_posix()})
                    else:
                        skipped.append({"target": target, "reason": "exists", "source": fallback.as_posix()})
                elif fallback_texture and fallback_texture.is_file() and not args.strict:
                    if copy_file(fallback_texture, destination, args.overwrite, args.dry_run):
                        copied.append({"target": target, "source": fallback_texture.as_posix()})
                        default_fallbacks.append({"target": target, "source": fallback_texture.as_posix()})
                    else:
                        skipped.append({"target": target, "reason": "exists", "source": fallback_texture.as_posix()})
                elif args.strict:
                    raise SystemExit(f"No usable PNG source or game fallback for target texture: {target}")

        if args.copy_unmapped:
            mapped_sources = {entry["source"] for entry in copied}
            for image in images:
                if image.relative_path in mapped_sources:
                    continue
                target = output / "extras" / (safe_name(image.logical_name) + image.path.suffix.lower())
                if copy_file(image.path, target, args.overwrite, args.dry_run):
                    copied.append({"target": target.relative_to(output).as_posix(), "source": image.relative_path})

        if not args.dry_run:
            output.mkdir(parents=True, exist_ok=True)
            (output / "texture_pack.conf").write_text(
                f"name = {args.pack_name}\n"
                f"description = Converted texture overrides for NavyCraft/Luanti from {source.name}\n",
                encoding="utf-8",
            )
            copied_license_files: list[str] = []
            for path in license_files:
                license_target = output / "licenses" / safe_name(path.relative_to(root).as_posix())
                license_target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, license_target)
                copied_license_files.append(license_target.relative_to(output).as_posix())

            attribution_lines = [
                "Converted texture pack",
                f"Source: {source}",
                "",
                "License check:",
                "Keep the source pack license and attribution with this folder before redistribution.",
                "",
                "Detected license/readme files:",
            ]
            attribution_lines.extend(f"- {path.relative_to(root).as_posix()}" for path in license_files)
            if not license_files:
                attribution_lines.append("- none detected")
            (output / "ATTRIBUTION.txt").write_text("\n".join(attribution_lines) + "\n", encoding="utf-8")

            report = {
                "source": str(source),
                "pack_name": args.pack_name,
                "mapping": str(mapping_path),
                "block_maps": [str(path) for path in block_map_paths],
                "copied": copied,
                "skipped": skipped,
                "fallbacks_from_game": fallbacks,
                "fallbacks_from_default_texture": default_fallbacks,
                "detected_images": len(images),
                "detected_models": len(models),
                "model_note": "Minecraft JSON models are not directly converted to Luanti meshes; inspect model_inventory for manual handling.",
                "model_inventory": [model.relative_path for model in models[:500]],
                "license_files": [path.relative_to(root).as_posix() for path in license_files],
                "copied_license_files": copied_license_files,
            }
            (output / "conversion_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

        print(f"Source images: {len(images)}")
        print(f"Copied files: {len(copied)}")
        print(f"Game fallback files: {len(fallbacks)}")
        print(f"Default fallback files: {len(default_fallbacks)}")
        print(f"Detected model files: {len(models)}")
        print(f"Output: {output}")
        return 0
    finally:
        if temp is not None:
            temp.cleanup()


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", "-i", required=True, type=Path, help="Minecraft-style pack folder or .zip file")
    parser.add_argument("--output", "-o", required=True, type=Path, help="Output Luanti texture-pack folder")
    parser.add_argument("--pack-name", default="navycraft_converted_pack", help="Name written to texture_pack.conf")
    parser.add_argument("--mapping", type=Path, default=script_dir / "default_mapping.json", help="JSON target/candidate mapping")
    parser.add_argument("--block-map", type=Path, default=DEFAULT_BLOCK_MAP, help="Generated NavyCraft block mapping JSON")
    parser.add_argument("--custom-blocks", action="append", type=Path, default=[], help="Additional block mapping JSON file; can be used more than once")
    parser.add_argument("--fallback-texture", type=Path, default=DEFAULT_FALLBACK_TEXTURE, help="Texture copied for required targets missing from the source pack")
    parser.add_argument("--game-dir", type=Path, default=DEFAULT_GAME_DIR, help="NavyCraft game directory used to discover required texture names and fallbacks")
    parser.add_argument("--overwrite", action="store_true", help="Replace existing files in the output folder")
    parser.add_argument("--copy-unmapped", action="store_true", help="Copy unmapped images into output/extras for later manual mapping")
    parser.add_argument("--require-license", action="store_true", help="Fail when no LICENSE/README/COPYING file is found in the source pack")
    parser.add_argument("--strict", action="store_true", help="Fail if a required NavyCraft texture cannot be produced")
    parser.add_argument("--dry-run", action="store_true", help="Report what would be converted without writing files")
    return parser.parse_args()


if __name__ == "__main__":
    raise SystemExit(convert(parse_args()))
