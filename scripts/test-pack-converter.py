#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONVERTER = ROOT / "tools" / "pack_converter" / "convert_pack.py"


def write_png(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        b"\x00\x00\x00\rIHDR"
        b"\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00"
        b"\x1f\x15\xc4\x89"
        b"\x00\x00\x00\rIDATx\x9cc\xf8\xff\xff?\x00\x05\xfe\x02\xfe"
        b"\xdc\xccY\xe7"
        b"\x00\x00\x00\x00IEND\xaeB`\x82"
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="navycraft-converter-test-") as tmp:
        root = Path(tmp)
        pack = root / "source_pack"
        output = root / "converted"
        write_png(pack / "assets" / "minecraft" / "textures" / "block" / "iron_block.png")
        write_png(pack / "assets" / "minecraft" / "textures" / "block" / "oak_planks.png")
        write_png(pack / "assets" / "minecraft" / "textures" / "particle" / "flame.png")
        write_png(pack / "assets" / "custompack" / "textures" / "block" / "armour_plate.png")
        (pack / "LICENSE.txt").write_text("Test license\n", encoding="utf-8")
        (pack / "assets" / "minecraft" / "models" / "block").mkdir(parents=True)
        (pack / "assets" / "minecraft" / "models" / "block" / "cube.json").write_text("{}", encoding="utf-8")
        custom_blocks = root / "custom_blocks.json"
        custom_blocks.write_text(json.dumps({
            "blocks": [
                {
                    "display_name": "Armour Plate",
                    "node_name": "nc_blocks:armour_plate",
                    "texture": "nc_block_armour_plate.png",
                    "source_textures": ["custompack:block/armour_plate"],
                }
            ]
        }), encoding="utf-8")

        subprocess.run(
            [
                sys.executable,
                str(CONVERTER),
                "--input",
                str(pack),
                "--output",
                str(output),
                "--pack-name",
                "test_pack",
                "--require-license",
                "--custom-blocks",
                str(custom_blocks),
            ],
            check=True,
            cwd=ROOT,
        )

        expected = ["nc_frame.png", "nc_helm_top.png", "nc_helm_side.png", "nc_fire.png"]
        for filename in expected:
            if not (output / filename).is_file():
                raise AssertionError(f"missing converted file: {filename}")

        report = json.loads((output / "conversion_report.json").read_text(encoding="utf-8"))
        if report["detected_images"] != 4:
            raise AssertionError(report)
        if report["detected_models"] != 1:
            raise AssertionError(report)
        if "LICENSE.txt" not in report["license_files"]:
            raise AssertionError(report)
        if not (output / "licenses" / "LICENSE.txt").is_file():
            raise AssertionError("license was not copied into output")

        game_textures = {
            path.name
            for path in (ROOT / "game" / "navycraft").rglob("*")
            if path.is_file() and path.parent.name == "textures" and path.suffix.lower() == ".png"
        }
        for filename in game_textures:
            if not (output / filename).is_file():
                raise AssertionError(f"required NavyCraft texture was not produced: {filename}")

        block_map = json.loads((ROOT / "data" / "navycraft_block_map.json").read_text(encoding="utf-8"))
        if len(block_map["blocks"]) < 400:
            raise AssertionError("expected a full legacy block mapping set")
        for block in block_map["blocks"]:
            if "minecraft" in block["node_name"]:
                raise AssertionError(f"game node name used forbidden namespace: {block}")
            if not (output / block["texture"]).is_file():
                raise AssertionError(f"required mapped block texture was not produced: {block['texture']}")
        registry = (ROOT / "game" / "navycraft" / "mods" / "nc_blocks" / "block_registry.lua").read_text(encoding="utf-8")
        if registry.count("texture=") != len(block_map["blocks"]):
            raise AssertionError("Lua registry and JSON block map are out of sync")
        if not (output / "nc_block_armour_plate.png").is_file():
            raise AssertionError("custom block mapping did not produce its output texture")

        zip_path = root / "source_pack.zip"
        with zipfile.ZipFile(zip_path, "w") as archive:
            for path in pack.rglob("*"):
                if path.is_file():
                    archive.write(path, path.relative_to(pack).as_posix())
        zipped_output = root / "converted_zip"
        subprocess.run(
            [
                sys.executable,
                str(CONVERTER),
                "--input",
                str(zip_path),
                "--output",
                str(zipped_output),
                "--pack-name",
                "test_pack_zip",
                "--require-license",
            ],
            check=True,
            cwd=ROOT,
        )
        if not (zipped_output / "nc_frame.png").is_file():
            raise AssertionError("zip input did not produce a usable texture pack")

    print("Pack converter test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
