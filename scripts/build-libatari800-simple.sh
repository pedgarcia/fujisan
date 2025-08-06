#!/bin/bash
# Simple script to verify libatari800.a exists and exit successfully
set -e

echo "=== Simple libatari800 verification ==="
ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
fi

cd "$ATARI800_SRC_PATH"

# Check if libatari800.a exists
if [ -f "src/libatari800.a" ]; then
    echo "=== libatari800.a already built successfully ==="
    ls -la src/libatari800.a
    echo "Library size: $(stat -c%s src/libatari800.a 2>/dev/null || stat -f%z src/libatari800.a 2>/dev/null || echo 'unknown') bytes"
    echo "=== Build verification completed successfully ==="
    exit 0
else
    echo "ERROR: libatari800.a not found at src/libatari800.a"
    exit 1
fi