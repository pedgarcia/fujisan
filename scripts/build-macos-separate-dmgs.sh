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
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build directories before starting"
            echo "  --skip-arm64  Skip ARM64 build"
            echo "  --skip-x86_64 Skip x86_64 build"
            echo "  --skip-dmg    Skip DMG creation"
            echo "  --help        Show this help message"
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
    
    # Sign
    echo_info "Signing app..."
    codesign --force --deep --sign - "Fujisan.app"
    
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
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              "$PROJECT_ROOT"
        
        echo_info "Building..."
        make -j$(sysctl -n hw.ncpu)
    else
        echo_info "Using existing x86_64 build"
    fi
    
    # Deploy Qt frameworks
    echo_info "Deploying Qt frameworks..."
    "$QT_X86_64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Sign
    echo_info "Signing app..."
    codesign --force --deep --sign - "Fujisan.app"
    
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
echo ""
echo "Users should download:"
echo "  • Apple Silicon Macs (M1/M2/M3): Fujisan-${PROJECT_VERSION}-arm64.dmg"
echo "  • Intel Macs: Fujisan-${PROJECT_VERSION}-x86_64.dmg"