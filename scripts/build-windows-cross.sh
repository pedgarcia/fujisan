#!/bin/bash
# build-windows-cross.sh - Cross-compile Fujisan for Windows using Podman

set -e

echo "=== Fujisan Windows Cross-compilation Build Script ==="

# Configuration
CONTAINER_IMAGE="maxrd2/arch-mingw"
BUILD_DIR="build-cross-windows"
CURRENT_DIR=$(pwd)

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

echo "Using container image: $CONTAINER_IMAGE"
echo "Build directory: $BUILD_DIR"
echo "Source directory: $CURRENT_DIR"

# Create CMake toolchain file for cross-compilation
cat > "$BUILD_DIR/mingw-toolchain.cmake" << 'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compilation tools
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where to find libraries and headers
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Qt5 configuration for cross-compilation
set(CMAKE_PREFIX_PATH /usr/x86_64-w64-mingw32)
set(Qt5_DIR /usr/x86_64-w64-mingw32/lib/cmake/Qt5)

# Windows-specific settings
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
EOF

echo "Created CMake toolchain file"

# Run cross-compilation build inside container
echo "Starting cross-compilation build..."

podman run --rm \
    -v "$CURRENT_DIR:/work" \
    --userns=keep-id \
    --workdir="/work" \
    "$CONTAINER_IMAGE" \
    bash -c "
        set -e
        echo '=== Cross-compilation Environment ==='
        x86_64-w64-mingw32-gcc --version
        x86_64-w64-mingw32-cmake --version
        
        echo '=== Configuring CMake ==='
        cd $BUILD_DIR
        x86_64-w64-mingw32-cmake \
            -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DFUJISAN_VERSION=dev-cross \
            ..
        
        echo '=== Building ==='
        make -j4 VERBOSE=1
        
        echo '=== Build Complete ==='
        ls -la
        file *.exe 2>/dev/null || echo 'No .exe files found'
    "

echo "Cross-compilation build completed!"
echo "Check $BUILD_DIR/ for results"