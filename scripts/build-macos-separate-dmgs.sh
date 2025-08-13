#!/bin/bash
#
# build-macos-separate-dmgs.sh - Build separate DMGs for Intel and Apple Silicon
#
# Creates two DMGs:
# - Fujisan-{version}-arm64.dmg   - For Apple Silicon Macs
# - Fujisan-{version}-x86_64.dmg  - For Intel Macs
#
# Each contains a properly configured Fujisan.app with native frameworks
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== macOS Platform-Specific Build for Fujisan ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"
DIST_DIR="${PROJECT_ROOT}/dist/macos"

# Qt paths
QT_ARM64_PATH="/opt/homebrew/opt/qt@5"
QT_X86_64_PATH="/usr/local/opt/qt@5"

# Version info
PROJECT_VERSION=$(git describe --tags --always 2>/dev/null || echo "1.0.0")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

echo_step() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Parse command line arguments
CLEAN_BUILD=false
SKIP_ARM64=false
SKIP_X86_64=false
SKIP_DMG=false
SIGN_BUILD=false
DEVELOPER_ID=""

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
        --skip-dmg)
            SKIP_DMG=true
            shift
            ;;
        --sign)
            SIGN_BUILD=true
            shift
            ;;
        --developer-id)
            DEVELOPER_ID="$2"
            SIGN_BUILD=true
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean              Clean build directories before starting"
            echo "  --skip-arm64         Skip ARM64 build"
            echo "  --skip-x86_64        Skip x86_64 build"
            echo "  --skip-dmg           Skip DMG creation"
            echo "  --sign               Sign with Developer ID Application certificate"
            echo "  --developer-id ID    Specify Developer ID certificate name"
            echo "  --help               Show this help message"
            echo ""
            echo "Output:"
            echo "  dist/Fujisan-{version}-arm64.dmg   - Apple Silicon DMG"
            echo "  dist/Fujisan-{version}-x86_64.dmg  - Intel DMG"
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
    rm -rf "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR"
fi

# Create directories
mkdir -p "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR" "$DIST_DIR"

# Function to find Developer ID certificate
find_developer_id() {
    if [[ -n "$DEVELOPER_ID" ]]; then
        echo "$DEVELOPER_ID"
        return
    fi
    
    echo_info "Looking for Developer ID Application certificate..."
    
    # Get certificate hash (more reliable than name)
    local cert_hash="ZC4PGBX6D6"  # Use Team ID as fallback
    
    # Try to get actual hash first
    local actual_hash=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}')
    
    if [[ -n "$actual_hash" ]]; then
        # Test if the hash works
        if codesign --verify --sign "$actual_hash" /dev/null 2>/dev/null; then
            cert_hash="$actual_hash"
        fi
    fi
    
    if [[ -z "$cert_hash" ]]; then
        echo_error "No Developer ID Application certificate found"
        echo_info "Available certificates:"
        security find-identity -v -p codesigning
        echo_info "Install a Developer ID Application certificate or run without --sign"
        exit 1
    fi
    
    # Get the friendly name for display
    local cert_name=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')
    
    echo_success "Found certificate: $cert_name"
    echo_info "Using certificate hash: $cert_hash"
    
    # Return the hash (which codesign prefers)
    echo "$cert_hash"
}

# Function to sign app bundle with hardened runtime
sign_app_bundle() {
    local app_path="$1"
    local dev_id="$2"
    
    echo_info "Signing app bundle with hardened runtime: $(basename "$app_path")"
    
    # Create entitlements for hardened runtime
    local entitlements="/tmp/fujisan_entitlements.plist"
    cat > "$entitlements" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
</dict>
</plist>
EOF
    
    # Sign all frameworks and libraries first
    if [[ -d "$app_path/Contents/Frameworks" ]]; then
        find "$app_path/Contents/Frameworks" -name "*.dylib" -o -name "*.framework" | while read framework; do
            codesign --force --timestamp --options runtime --sign "$dev_id" "$framework" 2>/dev/null || true
        done
    fi
    
    # Sign plugins
    if [[ -d "$app_path/Contents/PlugIns" ]]; then
        find "$app_path/Contents/PlugIns" -name "*.dylib" | while read plugin; do
            codesign --force --timestamp --options runtime --sign "$dev_id" "$plugin" 2>/dev/null || true
        done
    fi
    
    # Sign the main executable
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path/Contents/MacOS/Fujisan"
    
    # Sign the entire app bundle
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path"
    
    # Clean up
    rm -f "$entitlements"
    
    # Verify the signature
    codesign --verify --deep --strict --verbose=2 "$app_path"
    
    # Check for hardened runtime
    if codesign --display --verbose=2 "$app_path" 2>&1 | grep -q "runtime"; then
        echo_success "Hardened runtime enabled"
    else
        echo_error "Hardened runtime not detected"
    fi
    
    echo_success "App bundle signed: $(basename "$app_path")"
}

# Function to create DMG
create_dmg() {
    local APP_PATH="$1"
    local DMG_NAME="$2"
    local VOLUME_NAME="$3"
    
    if [[ ! -d "$APP_PATH" ]]; then
        echo_error "App not found at $APP_PATH"
        return 1
    fi
    
    echo_info "Creating DMG: $DMG_NAME"
    
    # Create temporary directory for DMG
    local TEMP_DIR=$(mktemp -d)
    
    # Copy app to temp directory
    cp -R "$APP_PATH" "$TEMP_DIR/Fujisan.app"
    
    # Create Applications symlink
    ln -s /Applications "$TEMP_DIR/Applications"
    
    # Create README if desired
    cat > "$TEMP_DIR/README.txt" << EOF
Fujisan v${PROJECT_VERSION}
========================

To install:
1. Drag Fujisan.app to the Applications folder
2. Double-click to launch

This is a native build for your Mac architecture.

For more information, visit:
https://github.com/atari800/fujisan
EOF
    
    # Create DMG
    hdiutil create -volname "$VOLUME_NAME" \
                   -srcfolder "$TEMP_DIR" \
                   -ov -format UDZO \
                   "$DIST_DIR/$DMG_NAME"
    
    # Clean up
    rm -rf "$TEMP_DIR"
    
    echo_success "Created: $DIST_DIR/$DMG_NAME"
    
    # Generate checksum
    cd "$DIST_DIR"
    shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
    echo "  SHA256: $(awk '{print $1}' "${DMG_NAME}.sha256")"
    cd - > /dev/null
}

# Build ARM64 version
if [[ "$SKIP_ARM64" == "false" ]]; then
    echo_step "Building Apple Silicon (ARM64) Version"
    
    cd "$ARM64_BUILD_DIR"
    
    # Build if needed
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]] || [[ "$CLEAN_BUILD" == "true" ]]; then
        echo_info "Configuring with CMake..."
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=arm64 \
              -DCMAKE_PREFIX_PATH="$QT_ARM64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              "$PROJECT_ROOT"
        
        echo_info "Building..."
        make -j$(sysctl -n hw.ncpu)
    else
        echo_info "Using existing ARM64 build"
    fi
    
    # Deploy Qt frameworks
    echo_info "Deploying Qt frameworks..."
    "$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Copy images to app bundle
    echo_info "Copying images to app bundle..."
    mkdir -p "Fujisan.app/Contents/Resources/images"
    cp -r "$PROJECT_ROOT/images/"*.png "Fujisan.app/Contents/Resources/images/" 2>/dev/null || true
    
    # Sign with Developer ID or ad-hoc
    if [[ "$SIGN_BUILD" == "true" ]]; then
        local dev_id=$(find_developer_id)
        sign_app_bundle "Fujisan.app" "$dev_id"
    else
        echo_info "Ad-hoc signing app..."
        codesign --force --deep --sign - "Fujisan.app"
    fi
    
    # Verify
    echo_info "Verifying ARM64 build..."
    EXEC_ARCH=$(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")
    if [[ "$EXEC_ARCH" == "arm64" ]]; then
        echo_success "ARM64 executable confirmed"
    else
        echo_error "Unexpected architecture: $EXEC_ARCH"
    fi
    
    if [[ -f "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" ]]; then
        QT_ARCH=$(lipo -archs "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" 2>/dev/null || echo "unknown")
        if [[ "$QT_ARCH" == "arm64" ]]; then
            echo_success "ARM64 Qt frameworks confirmed"
        else
            echo_info "Qt framework architecture: $QT_ARCH"
        fi
    fi
    
    # Create ARM64 DMG
    if [[ "$SKIP_DMG" == "false" ]]; then
        echo ""
        create_dmg "$ARM64_BUILD_DIR/Fujisan.app" \
                   "Fujisan-${PROJECT_VERSION}-arm64.dmg" \
                   "Fujisan (Apple Silicon)"
    fi
    
    echo ""
fi

# Build x86_64 version
if [[ "$SKIP_X86_64" == "false" ]]; then
    echo_step "Building Intel (x86_64) Version"
    
    cd "$X86_64_BUILD_DIR"
    
    # Build if needed
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]] || [[ "$CLEAN_BUILD" == "true" ]]; then
        echo_info "Configuring with CMake..."
        # Disable SDL2 for x86_64 build as it's likely not installed
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE \
              "$PROJECT_ROOT"
        
        echo_info "Building..."
        make -j$(sysctl -n hw.ncpu)
    else
        echo_info "Using existing x86_64 build"
    fi
    
    # Deploy Qt frameworks
    echo_info "Deploying Qt frameworks..."
    "$QT_X86_64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Copy images to app bundle
    echo_info "Copying images to app bundle..."
    mkdir -p "Fujisan.app/Contents/Resources/images"
    cp -r "$PROJECT_ROOT/images/"*.png "Fujisan.app/Contents/Resources/images/" 2>/dev/null || true
    
    # Sign with Developer ID or ad-hoc
    if [[ "$SIGN_BUILD" == "true" ]]; then
        local dev_id=$(find_developer_id)
        sign_app_bundle "Fujisan.app" "$dev_id"
    else
        echo_info "Ad-hoc signing app..."
        codesign --force --deep --sign - "Fujisan.app"
    fi
    
    # Verify
    echo_info "Verifying x86_64 build..."
    EXEC_ARCH=$(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")
    if [[ "$EXEC_ARCH" == "x86_64" ]]; then
        echo_success "x86_64 executable confirmed"
    else
        echo_error "Unexpected architecture: $EXEC_ARCH"
    fi
    
    if [[ -f "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" ]]; then
        QT_ARCH=$(lipo -archs "Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore" 2>/dev/null || echo "unknown")
        if [[ "$QT_ARCH" == "x86_64" ]]; then
            echo_success "x86_64 Qt frameworks confirmed"
        else
            echo_info "Qt framework architecture: $QT_ARCH"
        fi
    fi
    
    # Create x86_64 DMG
    if [[ "$SKIP_DMG" == "false" ]]; then
        echo ""
        create_dmg "$X86_64_BUILD_DIR/Fujisan.app" \
                   "Fujisan-${PROJECT_VERSION}-x86_64.dmg" \
                   "Fujisan (Intel)"
    fi
    
    echo ""
fi

# Final summary
echo_step "Build Complete"

if [[ -f "$DIST_DIR/Fujisan-${PROJECT_VERSION}-arm64.dmg" ]]; then
    SIZE=$(du -h "$DIST_DIR/Fujisan-${PROJECT_VERSION}-arm64.dmg" | cut -f1)
    echo_success "Apple Silicon DMG: Fujisan-${PROJECT_VERSION}-arm64.dmg ($SIZE)"
fi

if [[ -f "$DIST_DIR/Fujisan-${PROJECT_VERSION}-x86_64.dmg" ]]; then
    SIZE=$(du -h "$DIST_DIR/Fujisan-${PROJECT_VERSION}-x86_64.dmg" | cut -f1)
    echo_success "Intel DMG: Fujisan-${PROJECT_VERSION}-x86_64.dmg ($SIZE)"
fi

echo ""
echo "Distribution files are in: $DIST_DIR"
if [[ "$SIGN_BUILD" == "true" ]]; then
    echo "DMGs are signed with Developer ID and ready for notarization!"
    echo "Next step: Run ./scripts/sign-and-notarize-dmgs.sh --skip-signing"
else
    echo "DMGs use ad-hoc signing (for development only)"
    echo "For distribution: Rebuild with --sign and notarize"
fi

echo ""
echo "Users should download:"
echo "  • Apple Silicon Macs (M1/M2/M3): Fujisan-${PROJECT_VERSION}-arm64.dmg"
echo "  • Intel Macs: Fujisan-${PROJECT_VERSION}-x86_64.dmg"