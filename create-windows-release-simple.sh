#!/bin/bash
# Simple wrapper that just calls the simplified Windows build

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Just use the simplified build script
./scripts/build-windows-simple.sh

echo ""
echo "Windows release package is ready in: build-windows/"