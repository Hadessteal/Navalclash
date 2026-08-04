#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/upstream/luanti"
mkdir -p "$ROOT/upstream"
if [[ -d "$DEST/.git" ]]; then
  echo "Luanti checkout already exists: $DEST"
  exit 0
fi
git clone --depth 1 --branch 5.16.1 https://github.com/luanti-org/luanti.git "$DEST"
echo "Pinned Luanti 5.16.1 at $DEST"
