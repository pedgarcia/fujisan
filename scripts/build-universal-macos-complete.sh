#!/bin/bash
#
# build-universal-macos-complete.sh - Build Universal macOS Fujisan with Universal Qt Frameworks
#
# This script builds Fujisan for both ARM64 and x86_64 architectures,
# creates universal binaries for both the app and Qt frameworks,
# and packages everything into a distributable .app bundle.
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Universal macOS Build for Fujisan with Universal Qt Frameworks ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"
UNIVERSAL_BUILD_DIR="${PROJECT_ROOT}/build-universal"
DIST_DIR="${PROJECT_ROOT}/dist"

# Qt paths
QT_ARM64_PATH="/opt/homebrew/opt/qt@5"
QT_X86_64_PATH="/usr/local/opt/qt@5"

# Version info
PROJECT_VERSION=$(git describe --tags --always 2>/dev/null || echo "1.0.0")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_error() {
    echo -e "${RED}ERROR: $1${NC}" >&2
}

echo_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

echo_info() {
    echo -e "${YELLOW}→ $1${NC}"
}

# Clean up function
cleanup() {
    echo_info "Cleaning up build directories..."
    rm -rf "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$UNIVERSAL_BUILD_DIR"
}

# Parse command line arguments
CLEAN_BUILD=false
SKIP_ARM64=false
SKIP_X86_64=false
SKIP_SIGN=true  # Default to skip signing for now

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
        --sign)
            SKIP_SIGN=false
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build directories before starting"
            echo "  --skip-arm64  Skip ARM64 build (use existing)"
            echo "  --skip-x86_64 Skip x86_64 build (use existing)"
            echo "  --sign        Sign the application (requires certificates)"
            echo "  --help        Show this help message"
            exit 0
            ;;
        *)
            echo_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Verify Qt installations
echo_info "Checking Qt installations..."
if [[ ! -d "$QT_ARM64_PATH" ]]; then
    echo_error "ARM64 Qt not found at $QT_ARM64_PATH"
    echo "Please install with: brew install qt@5"
    exit 1
fi

if [[ ! -d "$QT_X86_64_PATH" ]]; then
    echo_error "x86_64 Qt not found at $QT_X86_64_PATH"
    echo "Please install with: arch -x86_64 /usr/local/bin/brew install qt@5"
    exit 1
fi

echo_success "Found ARM64 Qt at $QT_ARM64_PATH"
echo_success "Found x86_64 Qt at $QT_X86_64_PATH"
echo ""

# Clean build directories if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    cleanup
fi

# Create directories
mkdir -p "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$UNIVERSAL_BUILD_DIR" "$DIST_DIR"

# Build ARM64 version
if [[ "$SKIP_ARM64" == "false" ]]; then
    echo "=== Building ARM64 Version ==="
    
    cd "$ARM64_BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES=arm64 \
          -DCMAKE_PREFIX_PATH="$QT_ARM64_PATH" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
          "$PROJECT_ROOT"
    
    make -j$(sysctl -n hw.ncpu)
    
    echo_success "ARM64 build completed"
    echo "Binary architecture: $(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")"
    echo ""
fi

# Build x86_64 version
if [[ "$SKIP_X86_64" == "false" ]]; then
    echo "=== Building x86_64 Version ==="
    
    cd "$X86_64_BUILD_DIR"
    
    # Force x86_64 architecture for CMake - use system cmake which is universal
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES=x86_64 \
          -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
          "$PROJECT_ROOT"
    
    arch -x86_64 make -j$(sysctl -n hw.ncpu)
    
    echo_success "x86_64 build completed"
    echo "Binary architecture: $(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")"
    echo ""
fi

# Create universal binary
echo "=== Creating Universal Binary ==="

# Copy ARM64 app bundle as base
echo_info "Copying ARM64 app bundle as base..."
cp -R "$ARM64_BUILD_DIR/Fujisan.app" "$UNIVERSAL_BUILD_DIR/"

# Create universal binary for main executable
ARM64_BINARY="$ARM64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
X86_64_BINARY="$X86_64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
UNIVERSAL_BINARY="$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"

if [[ ! -f "$ARM64_BINARY" ]]; then
    echo_error "ARM64 binary not found at $ARM64_BINARY"
    exit 1
fi

if [[ ! -f "$X86_64_BINARY" ]]; then
    echo_error "x86_64 binary not found at $X86_64_BINARY"
    exit 1
fi

echo_info "Creating universal executable with lipo..."
lipo -create "$ARM64_BINARY" "$X86_64_BINARY" -output "$UNIVERSAL_BINARY"
echo_success "Universal binary created: $(lipo -archs "$UNIVERSAL_BINARY")"

# Deploy Qt frameworks for both architectures
echo ""
echo "=== Deploying Qt Frameworks ==="

cd "$UNIVERSAL_BUILD_DIR"

# First, deploy ARM64 Qt frameworks
echo_info "Deploying ARM64 Qt frameworks..."
"$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app" -verbose=1

# Now we need to create universal Qt frameworks
echo_info "Creating universal Qt frameworks..."

# List of Qt frameworks to make universal
QT_FRAMEWORKS=(
    "QtCore"
    "QtGui"
    "QtWidgets"
    "QtPrintSupport"
    "QtDBus"
)

# Also check for platform plugins
PLATFORM_PLUGINS=(
    "libqcocoa.dylib"
)

# Create universal Qt frameworks
for framework in "${QT_FRAMEWORKS[@]}"; do
    FRAMEWORK_PATH="Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    
    if [[ -f "$FRAMEWORK_PATH" ]]; then
        echo_info "Processing ${framework}..."
        
        # Deploy x86_64 version to a temp location
        TEMP_X86_APP="${UNIVERSAL_BUILD_DIR}/temp_x86.app"
        cp -R "$X86_64_BUILD_DIR/Fujisan.app" "$TEMP_X86_APP"
        "$QT_X86_64_PATH/bin/macdeployqt" "$TEMP_X86_APP" -verbose=0 2>/dev/null || true
        
        X86_FRAMEWORK="${TEMP_X86_APP}/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
        
        if [[ -f "$X86_FRAMEWORK" ]]; then
            # Create universal framework
            TEMP_UNIVERSAL="${FRAMEWORK_PATH}.universal"
            lipo -create "$FRAMEWORK_PATH" "$X86_FRAMEWORK" -output "$TEMP_UNIVERSAL" 2>/dev/null || {
                echo_info "  Framework might already be universal or incompatible, keeping ARM64 version"
                continue
            }
            mv "$TEMP_UNIVERSAL" "$FRAMEWORK_PATH"
            echo_success "  Created universal ${framework}: $(lipo -archs "$FRAMEWORK_PATH" 2>/dev/null || echo "unknown")"
        else
            echo_info "  x86_64 ${framework} not found, keeping ARM64 version"
        fi
        
        # Clean up temp app
        rm -rf "$TEMP_X86_APP"
    fi
done

# Create universal platform plugins
echo_info "Processing platform plugins..."
for plugin in "${PLATFORM_PLUGINS[@]}"; do
    PLUGIN_PATH="Fujisan.app/Contents/PlugIns/platforms/${plugin}"
    
    if [[ -f "$PLUGIN_PATH" ]]; then
        echo_info "Processing ${plugin}..."
        
        # Get x86_64 version
        TEMP_X86_APP="${UNIVERSAL_BUILD_DIR}/temp_x86.app"
        cp -R "$X86_64_BUILD_DIR/Fujisan.app" "$TEMP_X86_APP"
        "$QT_X86_64_PATH/bin/macdeployqt" "$TEMP_X86_APP" -verbose=0 2>/dev/null || true
        
        X86_PLUGIN="${TEMP_X86_APP}/Contents/PlugIns/platforms/${plugin}"
        
        if [[ -f "$X86_PLUGIN" ]]; then
            # Create universal plugin
            TEMP_UNIVERSAL="${PLUGIN_PATH}.universal"
            lipo -create "$PLUGIN_PATH" "$X86_PLUGIN" -output "$TEMP_UNIVERSAL" 2>/dev/null || {
                echo_info "  Plugin might already be universal or incompatible, keeping ARM64 version"
                rm -rf "$TEMP_X86_APP"
                continue
            }
            mv "$TEMP_UNIVERSAL" "$PLUGIN_PATH"
            echo_success "  Created universal ${plugin}: $(lipo -archs "$PLUGIN_PATH" 2>/dev/null || echo "unknown")"
        else
            echo_info "  x86_64 ${plugin} not found, keeping ARM64 version"
        fi
        
        # Clean up temp app
        rm -rf "$TEMP_X86_APP"
    fi
done

# Fix library paths and rpaths
echo_info "Fixing library paths..."
"$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app" -verbose=0 2>/dev/null || true

# Sign the application if requested
if [[ "$SKIP_SIGN" == "false" ]]; then
    echo ""
    echo "=== Code Signing ==="
    echo_info "Applying ad-hoc signature..."
    codesign --force --deep --sign - "Fujisan.app"
    echo_success "Ad-hoc signature applied"
fi

# Verify the universal app
echo ""
echo "=== Verification ==="

echo_info "Main executable:"
file "Fujisan.app/Contents/MacOS/Fujisan"
echo "  Architectures: $(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")"

echo ""
echo_info "Qt Frameworks:"
for framework in "${QT_FRAMEWORKS[@]}"; do
    FRAMEWORK_PATH="Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    if [[ -f "$FRAMEWORK_PATH" ]]; then
        echo "  ${framework}: $(lipo -archs "$FRAMEWORK_PATH" 2>/dev/null || echo "not universal")"
    fi
done

echo ""
echo_info "Platform plugins:"
for plugin in "${PLATFORM_PLUGINS[@]}"; do
    PLUGIN_PATH="Fujisan.app/Contents/PlugIns/platforms/${plugin}"
    if [[ -f "$PLUGIN_PATH" ]]; then
        echo "  ${plugin}: $(lipo -archs "$PLUGIN_PATH" 2>/dev/null || echo "not universal")"
    fi
done

# Create DMG
echo ""
echo "=== Creating DMG ==="

DMG_NAME="Fujisan-${PROJECT_VERSION}-universal.dmg"
DMG_PATH="${DIST_DIR}/${DMG_NAME}"

echo_info "Creating DMG: ${DMG_NAME}"

# Create temporary DMG directory
DMG_TEMP="${UNIVERSAL_BUILD_DIR}/dmg-temp"
rm -rf "$DMG_TEMP"
mkdir -p "$DMG_TEMP"

# Copy app and create Applications symlink
cp -R "Fujisan.app" "$DMG_TEMP/"
ln -s /Applications "$DMG_TEMP/Applications"

# Create DMG
hdiutil create -volname "Fujisan" \
               -srcfolder "$DMG_TEMP" \
               -ov -format UDZO \
               "$DMG_PATH"

# Sign DMG if signing is enabled
if [[ "$SKIP_SIGN" == "false" ]]; then
    echo_info "Signing DMG..."
    codesign --force --sign - "$DMG_PATH"
fi

# Clean up temp directory
rm -rf "$DMG_TEMP"

echo_success "DMG created: $DMG_PATH"

# Generate checksums
echo_info "Generating checksum..."
cd "$DIST_DIR"
shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
echo "  SHA256: $(cat "${DMG_NAME}.sha256")"

# Final summary
echo ""
echo "=== Universal Build Complete ==="
echo_success "Universal app: $UNIVERSAL_BUILD_DIR/Fujisan.app"
echo_success "DMG: $DMG_PATH"
echo ""
echo "Binary architectures:"
echo "  Main executable: $(lipo -archs "$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan")"
echo ""
echo "The universal app is ready for distribution on both Intel and Apple Silicon Macs!"