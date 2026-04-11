#!/usr/bin/env bash
# Install Fujisan's canonical FujiNet-PC fnconfig into a bundle directory.
# Copies data/fnconfig-fujisan.ini to both:
#   <dest>/data/fnconfig.ini  — used at runtime when fnconfig_on_spifs=1
#   <dest>/fnconfig.ini     — same content; root copy for -c fnconfig.ini / user edits
#
# Usage: install-fujisan-fnconfig.sh <fujinet-pc-bundle-directory>

set -euo pipefail

if [[ $# -ne 1 ]] || [[ -z "${1:-}" ]]; then
    echo "Usage: $(basename "$0") <fujinet-pc-bundle-directory>" >&2
    exit 1
fi

DEST=$(cd "$1" && pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$ROOT/data/fnconfig-fujisan.ini"

if [[ ! -f "$SRC" ]]; then
    echo "$(basename "$0"): missing $SRC" >&2
    exit 1
fi

mkdir -p "$DEST/data"
cp "$SRC" "$DEST/data/fnconfig.ini"
cp "$SRC" "$DEST/fnconfig.ini"
echo "$(basename "$0"): installed Fujisan fnconfig -> $DEST/data/fnconfig.ini and $DEST/fnconfig.ini"
