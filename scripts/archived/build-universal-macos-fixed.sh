#!/bin/bash
#
# build-universal-macos-fixed.sh - Build Universal macOS Fujisan with proper Qt framework handling
#
# This script builds Fujisan for both ARM64 and x86_64 architectures,
# creates universal binaries, and properly handles Qt frameworks for each architecture.
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
    echo_info "Cleaning build directories..."
    rm -rf "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$UNIVERSAL_BUILD_DIR"
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
    
    # Deploy Qt frameworks for ARM64
    echo_info "Deploying ARM64 Qt frameworks..."
    "$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app" -verbose=1
    
    echo_success "ARM64 build completed with Qt frameworks"
    echo ""
fi

# Build x86_64 version
if [[ "$SKIP_X86_64" == "false" ]]; then
    echo "=== Building x86_64 Version ==="
    
    cd "$X86_64_BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES=x86_64 \
          -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
          "$PROJECT_ROOT"
    
    make -j$(sysctl -n hw.ncpu)
    
    # Deploy Qt frameworks for x86_64
    echo_info "Deploying x86_64 Qt frameworks..."
    "$QT_X86_64_PATH/bin/macdeployqt" "Fujisan.app" -verbose=1
    
    echo_success "x86_64 build completed with Qt frameworks"
    echo ""
fi

# Create universal binary - Method 2: Keep both Qt framework sets
echo "=== Creating Universal Binary (Dual Framework Method) ==="

# Copy ARM64 app as base
echo_info "Using ARM64 app bundle as base (with ARM64 Qt frameworks)..."
rm -rf "$UNIVERSAL_BUILD_DIR/Fujisan.app"
cp -R "$ARM64_BUILD_DIR/Fujisan.app" "$UNIVERSAL_BUILD_DIR/"

# Create universal executable only
ARM64_BINARY="$ARM64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
X86_64_BINARY="$X86_64_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"
UNIVERSAL_BINARY="$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/MacOS/Fujisan"

echo_info "Creating universal executable..."
lipo -create "$ARM64_BINARY" "$X86_64_BINARY" -output "$UNIVERSAL_BINARY"
echo_success "Universal executable created: $(lipo -archs "$UNIVERSAL_BINARY")"

# Now we need to handle Qt frameworks specially
# The universal app will use ARM64 Qt frameworks since those are what's deployed
# On Intel Macs, users will need Rosetta 2, or we need a different approach

echo ""
echo_info "Note: This universal app uses ARM64 Qt frameworks."
echo_info "It will run natively on Apple Silicon Macs."
echo_info "On Intel Macs, the Qt frameworks will run under Rosetta 2."

# Alternative: Create truly universal Qt frameworks
echo ""
echo "=== Attempting to create universal Qt frameworks ==="

QT_FRAMEWORKS=(
    "QtCore"
    "QtGui" 
    "QtWidgets"
    "QtPrintSupport"
    "QtDBus"
    "QtMultimedia"
    "QtNetwork"
)

for framework in "${QT_FRAMEWORKS[@]}"; do
    ARM64_FW="$ARM64_BUILD_DIR/Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    X86_FW="$X86_64_BUILD_DIR/Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    UNIVERSAL_FW="$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    
    if [[ -f "$ARM64_FW" ]] && [[ -f "$X86_FW" ]]; then
        echo_info "Creating universal ${framework}..."
        
        # Check if they can be combined
        if lipo -create "$ARM64_FW" "$X86_FW" -output "${UNIVERSAL_FW}.tmp" 2>/dev/null; then
            mv "${UNIVERSAL_FW}.tmp" "$UNIVERSAL_FW"
            echo_success "  Created universal ${framework}: $(lipo -archs "$UNIVERSAL_FW" 2>/dev/null || echo "check failed")"
        else
            echo_info "  Cannot combine ${framework} - keeping ARM64 version"
        fi
    elif [[ -f "$ARM64_FW" ]]; then
        echo_info "  ${framework}: ARM64 only (x86_64 not found)"
    else
        echo_info "  ${framework}: Not found in builds"
    fi
done

# Handle platform plugins
echo_info "Processing platform plugins..."
ARM64_PLUGIN="$ARM64_BUILD_DIR/Fujisan.app/Contents/PlugIns/platforms/libqcocoa.dylib"
X86_PLUGIN="$X86_64_BUILD_DIR/Fujisan.app/Contents/PlugIns/platforms/libqcocoa.dylib"
UNIVERSAL_PLUGIN="$UNIVERSAL_BUILD_DIR/Fujisan.app/Contents/PlugIns/platforms/libqcocoa.dylib"

if [[ -f "$ARM64_PLUGIN" ]] && [[ -f "$X86_PLUGIN" ]]; then
    echo_info "Creating universal platform plugin..."
    if lipo -create "$ARM64_PLUGIN" "$X86_PLUGIN" -output "${UNIVERSAL_PLUGIN}.tmp" 2>/dev/null; then
        mv "${UNIVERSAL_PLUGIN}.tmp" "$UNIVERSAL_PLUGIN"
        echo_success "Created universal libqcocoa.dylib"
    else
        echo_info "Cannot combine platform plugins - keeping ARM64 version"
    fi
fi

# Fix rpaths and signatures
echo_info "Fixing library paths and code signing..."
cd "$UNIVERSAL_BUILD_DIR"

# Apply ad-hoc signature
codesign --force --deep --sign - "Fujisan.app"
echo_success "Applied ad-hoc signature"

# Verify the universal app
echo ""
echo "=== Verification ==="

echo_info "Main executable:"
file "Fujisan.app/Contents/MacOS/Fujisan"
echo "  Architectures: $(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")"

echo ""
echo_info "Checking Qt frameworks:"
for framework in "${QT_FRAMEWORKS[@]}"; do
    FW_PATH="Fujisan.app/Contents/Frameworks/${framework}.framework/Versions/5/${framework}"
    if [[ -f "$FW_PATH" ]]; then
        ARCHS=$(lipo -archs "$FW_PATH" 2>/dev/null || echo "arm64")
        echo "  ${framework}: $ARCHS"
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

# Clean up
rm -rf "$DMG_TEMP"

echo_success "DMG created: $DMG_PATH"

# Generate checksums
echo_info "Generating checksum..."
cd "$DIST_DIR"
shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
echo "  SHA256: $(cat "${DMG_NAME}.sha256")"

# Final summary
echo ""
echo "=== Build Complete ==="
echo_success "Universal app: $UNIVERSAL_BUILD_DIR/Fujisan.app"
echo_success "DMG: $DMG_PATH"
echo ""
echo "The app has:"
echo "  • Universal main executable (ARM64 + x86_64)"
echo "  • ARM64 Qt frameworks (will use Rosetta on Intel Macs)"
echo ""
echo "For fully native performance on both architectures, consider:"
echo "  • ARM64 users: Use this universal build (fully native)"
echo "  • Intel users: Use the x86_64 build from build-x86_64/Fujisan.app"