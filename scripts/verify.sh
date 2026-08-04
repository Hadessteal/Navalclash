#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$ROOT/prototype" -B "$ROOT/build/prototype" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/prototype" --parallel 2
ctest --test-dir "$ROOT/build/prototype" --output-on-failure
"$ROOT/build/prototype/navycraft_construct_benchmark"
cmake -S "$ROOT/platform" -B "$ROOT/build/platform" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/platform" --parallel 2
python3 -m py_compile "$ROOT/scripts/apply-engine-overlay.py" "$ROOT/scripts/test-overlay.py" "$ROOT/scripts/verify-upstream.py"
python3 "$ROOT/scripts/test-overlay.py"
python3 - "$ROOT" <<'PYVERIFY'
from pathlib import Path
import json
import sys
import yaml
root = Path(sys.argv[1])
json.loads((root / "native-engine-version.json").read_text(encoding="utf-8"))
for workflow in (root / ".github" / "workflows").glob("*.yml"):
    yaml.safe_load(workflow.read_text(encoding="utf-8"))
client = (root / "engine-overlay/src/navycraft/client/client_integration.cpp").read_text(encoding="utf-8")
server = (root / "engine-overlay/src/navycraft/server/server_integration.cpp").read_text(encoding="utf-8")
apply = (root / "scripts/apply-engine-overlay.py").read_text(encoding="utf-8")
assert "m_navycraft_handshake_accepted" in client
assert "IsNavyCraftPeerReady(target)" in server
assert "TOCLIENT_NUM_MSG_TYPES = 0x6D" in apply
assert "TOSERVER_NUM_MSG_TYPES = 0x57" in apply
assert "SendNavyCraftFullState(peer_id)" not in apply
handshake = (root / "engine-overlay/src/navycraft/construct/construct_handshake.h").read_text(encoding="utf-8")
script_api = (root / "engine-overlay/src/navycraft/script_api.cpp").read_text(encoding="utf-8")
bridge = (root / "game/navycraft/mods/nc_core/construct_bridge.lua").read_text(encoding="utf-8")
assert "CURRENT_PROTOCOL = 16" in handshake
assert "ConstructFeatureFinalPlayerPose" in handshake
assert "lua_pushinteger(L, 16)" in script_api
assert "local REQUIRED_PROTOCOL = 16" in bridge
assert apply.index("beginNavyCraftLocalPlayerMove") < apply.index("collisionMoveSimple")
assert "LocalPlayer final-pose moving-hull collision" in apply
assert "result.touching_ground);" not in apply
assert "\t\ttouching_ground);\n" in apply
PYVERIFY

if command -v texluac >/dev/null 2>&1; then
    while IFS= read -r -d '' file; do texluac -p "$file"; done < <(find "$ROOT/game/navycraft/mods" -name '*.lua' -print0)
fi
if command -v texlua >/dev/null 2>&1; then
    texlua "$ROOT/tests/lua_game_smoke.lua" "$ROOT/game/navycraft"
fi
