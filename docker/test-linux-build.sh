#!/bin/bash
#
# test-linux-build.sh - Test Linux build locally using Docker
#
# This script builds a Docker container matching the GitHub Actions Linux
# environment and tests the build process locally for debugging.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Testing Linux Build with Docker ==="
echo "Project root: $PROJECT_ROOT"

# Build container image with podman
echo "Building container image with podman..."
podman build -t fujisan-linux-test -f "$SCRIPT_DIR/Dockerfile.ubuntu" "$SCRIPT_DIR"

# Run the build in container
echo "Running Linux build test..."
podman run --rm \
    -v "$PROJECT_ROOT:/workspace" \
    -w /workspace \
    -e FUJISAN_VERSION="v1.0.0-test" \
    fujisan-linux-test \
    bash -c '
        echo "=== Docker Environment Info ==="
        echo "OS: $(cat /etc/os-release | grep PRETTY_NAME)"
        echo "CMAKE: $(cmake --version | head -1)"
        echo "Qt5: $(qmake -query QT_VERSION)"
        echo "GCC: $(gcc --version | head -1)"
        echo ""
        
        echo "=== Starting Build Process ==="
        
        # Create build directory
        mkdir -p build-release
        cd build-release
        
        # Configure CMake (this should download and build libatari800)
        echo "Configuring CMake..."
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_INSTALL_PREFIX=/usr \
              -DFUJISAN_VERSION="${FUJISAN_VERSION:-dev}" \
              ..
        
        echo "Building Fujisan..."
        make -j$(nproc)
        
        # Verify libatari800 integration
        echo "=== Verifying Build Results ==="
        if [ -f "atari800-src/src/libatari800.a" ]; then
            echo "✓ libatari800.a found and integrated"
            ls -lh atari800-src/src/libatari800.a
        else
            echo "❌ libatari800.a not found"
            echo "Looking for libatari800 files:"
            find . -name "*libatari800*" -o -name "*atari800*" | head -10
            exit 1
        fi
        
        # Test binary
        echo "Testing Fujisan binary..."
        if [ -f "Fujisan" ]; then
            echo "✓ Fujisan binary created"
            ldd Fujisan | head -10
            file Fujisan
            ./Fujisan --help || echo "(Help command may not be available)"
        else
            echo "❌ Fujisan binary not found"
            ls -la
            exit 1
        fi
        
        echo ""
        echo "=== Build Test Completed Successfully! ==="
    '

echo ""
echo "=== Docker Build Test Summary ==="
echo "If the build completed successfully, the Linux workflow should work."
echo "If there were errors, we can debug them locally before pushing fixes."