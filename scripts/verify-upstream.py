#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

EXPECTED_COMMIT = "5ebd9b57984d5854e0a37fd0125da48e9e59e190"
EXPECTED_BLOBS = {
    "src/CMakeLists.txt": "7ade36680373f472842fa5ace56f8e32f9f3ab81",
    "src/script/scripting_server.cpp": "8556422702455eacce88d4b414f465cbe4f03919",
    "src/network/networkprotocol.h": "e359a266cefb8d0e975e9bce6ce5bbebd82af12a",
    "src/network/clientopcodes.cpp": "77f4074cdae130f6023817fd3774319fdf08a8b0",
    "src/network/serveropcodes.cpp": "376289508cd4edb79f323ccd9b3b253ffb927552",
    "src/network/serverpackethandler.cpp": "b95379997f756d450cf73e96ace0d279bc18b5b1",
    "src/client/client.h": "30881c73614727ec39885a134b6610ea2aa9b6b7",
    "src/client/client.cpp": "a575cc5679e1d1f65d373211288d16121e24e290",
    "src/client/localplayer.cpp": "ccfb8be05d6da7d00efded62e20c00663ac65097",
    "src/server.h": "01064e9a8c1211e2f3e76bcfe936a7ab3aad2a19",
    "src/server.cpp": "36133d1b3a3b84bce93709d97b7fdba825ddb66f",
    "src/client/mapblock_mesh.h": "e70e9e268784e0c6ce5dc28d615aba80ffa6f8ac",
    "src/client/mapblock_mesh.cpp": "c4c45d9e24f08a0c1804c0bb2511855984ecb3c4",
}

if len(sys.argv) != 2:
    raise SystemExit("usage: verify-upstream.py PATH_TO_LUANTI")
root = Path(sys.argv[1]).resolve()
if not (root / "CMakeLists.txt").exists() or not (root / "src").is_dir():
    raise SystemExit(f"not a Luanti source tree: {root}")
try:
    actual = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
except Exception as exc:
    raise SystemExit(f"unable to read Luanti git revision: {exc}")
if actual != EXPECTED_COMMIT:
    raise SystemExit(
        f"wrong Luanti revision: expected {EXPECTED_COMMIT}, got {actual}"
    )
errors = []
for relative, expected in EXPECTED_BLOBS.items():
    path = root / relative
    if not path.is_file():
        errors.append(f"missing {relative}")
        continue
    try:
        actual_blob = subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", f"HEAD:{relative}"],
            text=True,
        ).strip()
    except Exception as exc:
        errors.append(f"{relative}: unable to read git object id: {exc}")
        continue
    if actual_blob != expected:
        errors.append(
            f"{relative}: expected git object {expected}, got {actual_blob}"
        )
if errors:
    raise SystemExit("Luanti 5.16.1 source files do not match the pinned release:\n  " + "\n  ".join(errors))
print(f"verified exact Luanti 5.16.1 commit and {len(EXPECTED_BLOBS)} upstream blobs")
