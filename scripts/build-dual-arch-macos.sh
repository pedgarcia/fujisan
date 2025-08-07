#!/bin/bash
#
# build-dual-arch-macos.sh - Build Fujisan for both architectures with separate bundles
#
# This creates two separate, properly configured app bundles:
# - Fujisan-arm64.app for Apple Silicon Macs
# - Fujisan-x86_64.app for Intel Macs
# Then packages both into a single DMG
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Dual Architecture macOS Build for Fujisan ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"
DIST_DIR="${PROJECT_ROOT}/dist"
PACKAGE_DIR="${PROJECT_ROOT}/build-package"

# Qt paths
QT_ARM64_PATH="/opt/homebrew/opt/qt@5"
QT_X86_64_PATH="/usr/local/opt/qt@5"

# Version info
PROJECT_VERSION=$(git describe --tags --always 2>/dev/null || echo "1.0.0")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

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
    rm -rf "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$PACKAGE_DIR"
fi

# Create directories
mkdir -p "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$DIST_DIR" "$PACKAGE_DIR"

# Build ARM64 version
if [[ "$SKIP_ARM64" == "false" ]]; then
    echo "=== Building ARM64 Version ==="
    
    cd "$ARM64_BUILD_DIR"
    
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]]; then
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=arm64 \
              -DCMAKE_PREFIX_PATH="$QT_ARM64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              "$PROJECT_ROOT"
        
        make -j$(sysctl -n hw.ncpu)
    fi
    
    # Deploy Qt frameworks for ARM64
    echo_info "Deploying ARM64 Qt frameworks..."
    "$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Verify
    echo_info "Verifying ARM64 build..."
    if file "Fujisan.app/Contents/MacOS/Fujisan" | grep -q "arm64"; then
        echo_success "ARM64 executable confirmed"
    fi
    
    # Check Qt framework
    if [[ -f "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" ]]; then
        if file "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" | grep -q "arm64"; then
            echo_success "ARM64 Qt frameworks deployed"
        fi
    fi
    
    echo ""
fi

# Build x86_64 version
if [[ "$SKIP_X86_64" == "false" ]]; then
    echo "=== Building x86_64 Version ==="
    
    cd "$X86_64_BUILD_DIR"
    
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]]; then
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              "$PROJECT_ROOT"
        
        make -j$(sysctl -n hw.ncpu)
    fi
    
    # Deploy Qt frameworks for x86_64
    echo_info "Deploying x86_64 Qt frameworks..."
    "$QT_X86_64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Verify
    echo_info "Verifying x86_64 build..."
    if file "Fujisan.app/Contents/MacOS/Fujisan" | grep -q "x86_64"; then
        echo_success "x86_64 executable confirmed"
    fi
    
    # Check Qt framework
    if [[ -f "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" ]]; then
        if file "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" | grep -q "x86_64"; then
            echo_success "x86_64 Qt frameworks deployed"
        fi
    fi
    
    echo ""
fi

# Create package with both apps
echo "=== Creating Distribution Package ==="

# Clean package directory
rm -rf "$PACKAGE_DIR"/*

# Copy both apps with architecture suffix
echo_info "Copying ARM64 app..."
cp -R "$ARM64_BUILD_DIR/Fujisan.app" "$PACKAGE_DIR/Fujisan-arm64.app"

echo_info "Copying x86_64 app..."
cp -R "$X86_64_BUILD_DIR/Fujisan.app" "$PACKAGE_DIR/Fujisan-x86_64.app"

# Create a launcher script
echo_info "Creating universal launcher..."
cat > "$PACKAGE_DIR/Fujisan-Launcher.command" << 'EOF'
#!/bin/bash
# Fujisan Universal Launcher
# Automatically launches the correct version for your Mac

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Detect architecture
ARCH=$(uname -m)

if [[ "$ARCH" == "arm64" ]]; then
    echo "Detected Apple Silicon Mac (ARM64)"
    APP="$SCRIPT_DIR/Fujisan-arm64.app"
elif [[ "$ARCH" == "x86_64" ]]; then
    echo "Detected Intel Mac (x86_64)"
    APP="$SCRIPT_DIR/Fujisan-x86_64.app"
else
    echo "Unknown architecture: $ARCH"
    exit 1
fi

if [[ -d "$APP" ]]; then
    echo "Launching $APP..."
    open "$APP"
else
    echo "Error: Application not found at $APP"
    exit 1
fi
EOF

chmod +x "$PACKAGE_DIR/Fujisan-Launcher.command"

# Create README
cat > "$PACKAGE_DIR/README.txt" << EOF
Fujisan - Universal macOS Distribution
======================================

This package contains native builds for both Intel and Apple Silicon Macs:

• Fujisan-arm64.app   - For Apple Silicon Macs (M1, M2, M3, etc.)
• Fujisan-x86_64.app  - For Intel Macs

Installation:
1. Drag the appropriate app to your Applications folder
   - Apple Silicon users: Use Fujisan-arm64.app
   - Intel users: Use Fujisan-x86_64.app

2. Or use Fujisan-Launcher.command to automatically launch the correct version

Note: Each app contains the appropriate Qt frameworks for its architecture,
ensuring native performance on your Mac.

Version: $PROJECT_VERSION
EOF

# Sign both apps
echo_info "Applying ad-hoc signatures..."
codesign --force --deep --sign - "$PACKAGE_DIR/Fujisan-arm64.app"
codesign --force --deep --sign - "$PACKAGE_DIR/Fujisan-x86_64.app"
echo_success "Apps signed"

# Create DMG
echo ""
echo "=== Creating DMG ==="

DMG_NAME="Fujisan-${PROJECT_VERSION}-universal.dmg"
DMG_PATH="${DIST_DIR}/${DMG_NAME}"

echo_info "Creating DMG: ${DMG_NAME}"

# Create DMG with both apps
hdiutil create -volname "Fujisan Universal" \
               -srcfolder "$PACKAGE_DIR" \
               -ov -format UDZO \
               "$DMG_PATH"

echo_success "DMG created: $DMG_PATH"

# Generate checksums
echo_info "Generating checksum..."
cd "$DIST_DIR"
shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
echo "  SHA256: $(cat "${DMG_NAME}.sha256")"

# Final summary
echo ""
echo "=== Build Complete ==="
echo_success "Package directory: $PACKAGE_DIR"
echo_success "DMG: $DMG_PATH"
echo ""
echo "The DMG contains:"
echo "  • Fujisan-arm64.app   - Native Apple Silicon app with ARM64 Qt"
echo "  • Fujisan-x86_64.app  - Native Intel app with x86_64 Qt"
echo "  • Fujisan-Launcher    - Auto-launcher script"
echo "  • README.txt          - Installation instructions"
echo ""
echo "Each app will run natively on its target architecture!"