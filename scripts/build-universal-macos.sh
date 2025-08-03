#!/bin/bash
#
# build-universal-macos.sh - Build Universal macOS Fujisan Binary
#
# This script builds Fujisan for both ARM64 and x86_64 architectures,
# then combines them into a universal binary using lipo.
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Universal macOS Build for Fujisan ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"
UNIVERSAL_BUILD_DIR="${PROJECT_ROOT}/build-universal"
DIST_DIR="${PROJECT_ROOT}/dist"

# Clean up function
cleanup() {
    echo "Cleaning up build directories..."
    rm -rf "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$UNIVERSAL_BUILD_DIR"
}

# Parse command line arguments
CLEAN_BUILD=false
SKIP_ARM64=false
SKIP_X86_64=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --skip-arm64)
            SKIP_ARM64=true
            shift
            ;;
        --skip-x86_64)
            SKIP_X86_64=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build directories before starting"
            echo "  --skip-arm64  Skip ARM64 build (use existing)"
            echo "  --skip-x86_64 Skip x86_64 build (use existing)"
            echo "  --help        Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Clean build directories if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    cleanup
fi

# Create directories
mkdir -p "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$UNIVERSAL_BUILD_DIR" "$DIST_DIR"

# Build ARM64 version
if [[ "$SKIP_ARM64" == "false" ]]; then
    echo "=== Building ARM64 Version ==="
    
    # Clear environment variables that might interfere
    unset CMAKE_PREFIX_PATH
    unset Qt5_DIR
    unset TARGET_ARCH
    
    export TARGET_ARCH="arm64"
    export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5"
    
    cd "$ARM64_BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES=arm64 \
          -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
          -DCMAKE_C_COMPILER=clang \
          -DCMAKE_CXX_COMPILER=clang++ \
          "$PROJECT_ROOT"
    
    make -j$(sysctl -n hw.ncpu)
    
    echo "ARM64 build completed"
    echo "Verifying ARM64 binary architecture:"
    lipo -archs "Fujisan.app/Contents/MacOS/Fujisan"
    echo ""
fi

# Build x86_64 version under Rosetta
if [[ "$SKIP_X86_64" == "false" ]]; then
    echo "=== Building x86_64 Version (under Rosetta) ==="
    
    # Clear environment variables that might interfere
    unset CMAKE_PREFIX_PATH
    unset Qt5_DIR
    unset TARGET_ARCH
    
    export TARGET_ARCH="x86_64"
    export CMAKE_PREFIX_PATH="/usr/local/opt/qt@5"
    export PATH="/usr/local/bin:$PATH"  # Use Intel Homebrew tools
    
    cd "$X86_64_BUILD_DIR"
    
    # Use x86_64 cmake and tools for proper x86_64 build
    arch -x86_64 /usr/local/bin/cmake -DCMAKE_BUILD_TYPE=Release \
                                      -DCMAKE_OSX_ARCHITECTURES=x86_64 \
                                      -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                                      -DCMAKE_C_COMPILER=clang \
                                      -DCMAKE_CXX_COMPILER=clang++ \
                                      "$PROJECT_ROOT"
    
    arch -x86_64 make -j$(sysctl -n hw.ncpu)
    
    echo "x86_64 build completed"
    echo "Verifying x86_64 binary architecture:"
    lipo -archs "Fujisan.app/Contents/MacOS/Fujisan"
    echo ""
fi

# Create universal binary
echo "=== Creating Universal Binary ==="

# Copy ARM64 app bundle as base
cp -R "$ARM64_BUILD_DIR/Fujisan.app" "$UNIVERSAL_BUILD_DIR/"

# Create universal binary using lipo
ARM64_BINARY="$ARM64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
X86_64_BINARY="$X86_64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
UNIVERSAL_BINARY="$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"

if [[ -f "$ARM64_BINARY" && -f "$X86_64_BINARY" ]]; then
    echo "Combining binaries with lipo..."
    lipo -create "$ARM64_BINARY" "$X86_64_BINARY" -output "$UNIVERSAL_BINARY"
    
    echo "Universal binary created successfully!"
    echo "Verifying universal binary architecture:"
    lipo -archs "$UNIVERSAL_BINARY"
    
    # Get binary info
    echo ""
    echo "Binary size information:"
    echo "ARM64 only:    $(du -h "$ARM64_BINARY" | cut -f1)"
    echo "x86_64 only:   $(du -h "$X86_64_BINARY" | cut -f1)"
    echo "Universal:     $(du -h "$UNIVERSAL_BINARY" | cut -f1)"
else
    echo "Error: Could not find both architecture binaries"
    echo "ARM64 binary: $ARM64_BINARY (exists: $(test -f "$ARM64_BINARY" && echo "yes" || echo "no"))"
    echo "x86_64 binary: $X86_64_BINARY (exists: $(test -f "$X86_64_BINARY" && echo "yes" || echo "no"))"
    exit 1
fi

# Deploy Qt frameworks for universal binary
echo ""
echo "=== Deploying Qt Frameworks ==="
cd "$UNIVERSAL_BUILD_DIR"

# Use ARM64 macdeployqt (should work for universal binaries)
"/opt/homebrew/opt/qt@5/bin/macdeployqt" "Fujisan.app"

echo ""
echo "=== Universal Build Summary ==="
echo "✓ ARM64 build: $ARM64_BUILD_DIR"
echo "✓ x86_64 build: $X86_64_BUILD_DIR"
echo "✓ Universal app: $UNIVERSAL_BUILD_DIR/Fujisan.app"
echo ""
echo "Fujisan universal binary ready for distribution!"
echo "Binary architectures: $(lipo -archs "$UNIVERSAL_BINARY")"

# Optionally copy to dist directory
if [[ -d "$DIST_DIR" ]]; then
    echo ""
    echo "Copying universal app to dist directory..."
    cp -R "$UNIVERSAL_BUILD_DIR/Fujisan.app" "$DIST_DIR/"
    echo "✓ Universal app copied to: $DIST_DIR/Fujisan.app"
fi