#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/stellarium-source"
  exit 2
fi

SRC="$(realpath "$1")"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [[ ! -f "$SRC/CMakeLists.txt" || ! -d "$SRC/plugins" ]]; then
  echo "Not a Stellarium source tree: $SRC" >&2
  exit 1
fi

rm -rf "$SRC/plugins/OdehContours"
mkdir -p "$SRC/plugins/OdehContours"
cp -a "$HERE/CMakeLists.txt" "$HERE/src" "$SRC/plugins/OdehContours/"

if ! grep -q 'ADD_PLUGIN(OdehContours' "$SRC/CMakeLists.txt"; then
  python3 - "$SRC/CMakeLists.txt" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
needle = "ADD_PLUGIN(SimpleDrawLine 0)"
insert = needle + "\nADD_PLUGIN(OdehContours 1)"
if needle not in s:
    raise SystemExit("Could not find ADD_PLUGIN(SimpleDrawLine 0) in root CMakeLists.txt")
p.write_text(s.replace(needle, insert, 1))
PY
fi

echo "Installed source into: $SRC/plugins/OdehContours"
echo "Registered: ADD_PLUGIN(OdehContours 1)"
echo "Now configure/build Stellarium from this exact source version."
