#!/bin/bash

# Complete Fujisan Build, Sign, and Notarize Script
# This script builds, signs with Developer ID, and creates DMGs ready for notarization

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${YELLOW}=== $1 ===${NC}"; }

# Configuration
BUNDLE_ID="com.atari.fujisan"
VERSION=$(git describe --tags --always 2>/dev/null || echo "1.0.0")

# Parse arguments
DEVELOPER_ID=""
SKIP_BUILD=false
SKIP_SIGNING=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --developer-id)
            DEVELOPER_ID="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-signing)
            SKIP_SIGNING=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --developer-id ID    Specify Developer ID certificate"
            echo "  --skip-build         Skip build step (use existing apps)"
            echo "  --skip-signing       Skip signing (for testing)"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Find Developer ID if not specified
find_developer_id() {
    if [[ -n "$DEVELOPER_ID" ]]; then
        echo "$DEVELOPER_ID"
        return
    fi
    
    log_info "Looking for Developer ID Application certificate..."
    
    # Get certificate hash (more reliable than name)
    local cert_hash=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}')
    
    if [[ -z "$cert_hash" ]]; then
        log_error "No Developer ID Application certificate found"
        log_info "Available certificates:"
        security find-identity -v -p codesigning
        log_info "Please install a Developer ID Application certificate or use --skip-signing"
        exit 1
    fi
    
    # Get the friendly name for display
    local cert_name=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')
    
    log_success "Found certificate: $cert_name"
    log_info "Using certificate hash: $cert_hash"
    
    # Return the hash (which codesign prefers)
    echo "$cert_hash"
}

# Sign app bundle with hardened runtime
sign_app_bundle() {
    local app_path="$1"
    local dev_id="$2"
    
    log_info "Signing app bundle: $(basename "$app_path")"
    
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
    
    # Sign all frameworks first
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
    
    # Sign main executable
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path/Contents/MacOS/Fujisan"
    
    # Sign the app bundle
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path"
    
    # Clean up
    rm -f "$entitlements"
    
    # Verify
    codesign --verify --deep --strict --verbose=2 "$app_path"
    log_success "App bundle signed: $(basename "$app_path")"
}

# Create DMG with signed app
create_signed_dmg() {
    local app_path="$1"
    local dmg_name="$2"
    local volume_name="$3"
    
    log_info "Creating DMG: $dmg_name"
    
    local temp_dir=$(mktemp -d)
    cp -R "$app_path" "$temp_dir/Fujisan.app"
    ln -s /Applications "$temp_dir/Applications"
    
    # Create DMG
    hdiutil create -volname "$volume_name" -srcfolder "$temp_dir" -ov -format UDZO "$PROJECT_ROOT/dist/$dmg_name"
    
    rm -rf "$temp_dir"
    log_success "Created: dist/$dmg_name"
}

# Main execution
main() {
    log_step "Fujisan Build and Sign Process"
    log_info "Version: $VERSION"
    
    # Create dist directory
    mkdir -p "$PROJECT_ROOT/dist"
    
    # Find Developer ID if signing
    local dev_id=""
    if [[ "$SKIP_SIGNING" == "false" ]]; then
        dev_id=$(find_developer_id)
    fi
    
    # Build if not skipping
    if [[ "$SKIP_BUILD" == "false" ]]; then
        log_step "Building Fujisan"
        cd "$PROJECT_ROOT"
        
        # Build ARM64
        log_info "Building ARM64 version..."
        rm -rf build-arm64
        mkdir build-arm64 && cd build-arm64
        
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=arm64 \
              -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              ..
        
        make -j$(sysctl -n hw.ncpu)
        "/opt/homebrew/opt/qt@5/bin/macdeployqt" "Fujisan.app"
        
        # Copy to final location and sign
        if [[ "$SKIP_SIGNING" == "false" ]]; then
            sign_app_bundle "Fujisan.app" "$dev_id"
        fi
        
        create_signed_dmg "Fujisan.app" "Fujisan-${VERSION}-arm64.dmg" "Fujisan (Apple Silicon)"
        
        cd "$PROJECT_ROOT"
        
        # Build x86_64
        log_info "Building x86_64 version..."
        rm -rf build-x86_64
        mkdir build-x86_64 && cd build-x86_64
        
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DCMAKE_PREFIX_PATH="/usr/local/opt/qt@5" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE \
              ..
        
        make -j$(sysctl -n hw.ncpu)
        "/usr/local/opt/qt@5/bin/macdeployqt" "Fujisan.app"
        
        # Copy to final location and sign
        if [[ "$SKIP_SIGNING" == "false" ]]; then
            sign_app_bundle "Fujisan.app" "$dev_id"
        fi
        
        create_signed_dmg "Fujisan.app" "Fujisan-${VERSION}-x86_64.dmg" "Fujisan (Intel)"
        
        cd "$PROJECT_ROOT"
    fi
    
    log_step "Build Complete"
    
    # Show results
    if [[ -f "dist/Fujisan-${VERSION}-arm64.dmg" ]]; then
        log_success "Apple Silicon DMG: dist/Fujisan-${VERSION}-arm64.dmg"
    fi
    
    if [[ -f "dist/Fujisan-${VERSION}-x86_64.dmg" ]]; then
        log_success "Intel DMG: dist/Fujisan-${VERSION}-x86_64.dmg"
    fi
    
    if [[ "$SKIP_SIGNING" == "false" ]]; then
        log_info "DMGs are signed and ready for notarization!"
        log_info "Next step: Run ./scripts/sign-and-notarize-dmgs.sh --skip-signing"
    else
        log_info "DMGs created without signing (use --skip-signing=false to sign)"
    fi
}

main
