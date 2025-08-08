#!/bin/bash
# Test Windows build script with verbose output

set -ex

echo "=== Test Windows Build ==="

PROJECT_ROOT="$(pwd)"

# Clean previous builds
rm -rf build-test-windows

echo "Starting test build..."

# Run build with verbose output and separate steps
podman run --rm \
    -v "$PROJECT_ROOT:/work" \
    --workdir="/work" \
    --platform linux/amd64 \
    maxrd2/arch-mingw \
    bash -c '
set -ex

echo "=== Step 1: Creating build directory ==="
rm -rf build-test-windows
mkdir -p build-test-windows
cd build-test-windows

echo "=== Step 2: Running CMake configure ==="
x86_64-w64-mingw32-cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    ..

echo "=== Step 3: Building with single job to see exact failure ==="
make VERBOSE=1 -j1

echo "=== Step 4: Checking outputs ==="
ls -la
find . -name "*.exe" -o -name "*.a" -o -name "*.dll" | head -20
'

echo "Test build complete!"