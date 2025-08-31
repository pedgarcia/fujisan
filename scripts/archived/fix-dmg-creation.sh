#!/bin/bash

# Fujisan DMG Creation Fix
# Run this to create DMGs from existing app bundles

set -e

PROJECT_ROOT="/Users/pgarcia/dev/atari/fujisan"
DIST_DIR="$PROJECT_ROOT/dist/macos"
VERSION=$(git describe --tags --always 2>/dev/null || echo "1.0.0")

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo_info() { echo -e "${YELLOW}→ $1${NC}"; }
echo_success() { echo -e "${GREEN}✓ $1${NC}"; }
echo_error() { echo -e "${RED}✗ $1${NC}"; }

# Function to create DMG safely
create_dmg_safely() {
    local app_path="$1"
    local dmg_name="$2"
    local volume_name="$3"
    
    echo_info "Creating DMG: $dmg_name"
    
    # Cleanup any existing mounts/files
    hdiutil detach "/Volumes/$volume_name" -force 2>/dev/null || true
    sleep 1
    rm -f "$DIST_DIR/$dmg_name"
    
    # Create temp directory
    local temp_dir=$(mktemp -d -t fujisan_dmg_XXXXXX)
    
    # Copy app and create symlink
    cp -pR "$app_path" "$temp_dir/Fujisan.app"
    ln -s /Applications "$temp_dir/Applications"
    
    # Simple README
    echo "Fujisan v$VERSION - Drag to Applications to install" > "$temp_dir/README.txt"
    
    # Create DMG
    if hdiutil create -volname "$volume_name" -srcfolder "$temp_dir" -ov -format UDZO "$DIST_DIR/$dmg_name"; then
        echo_success "Created: $dmg_name"
        # Generate checksum
        cd "$DIST_DIR" && shasum -a 256 "$dmg_name" > "${dmg_name}.sha256"
    else
        echo_error "Failed to create: $dmg_name"
        rm -rf "$temp_dir"
        return 1
    fi
    
    rm -rf "$temp_dir"
}

# Create dist directory
mkdir -p "$DIST_DIR"

# Check for existing app bundles
if [[ -d "$PROJECT_ROOT/build-arm64/Fujisan.app" ]]; then
    echo_info "Found ARM64 app bundle"
    create_dmg_safely "$PROJECT_ROOT/build-arm64/Fujisan.app" \
                     "Fujisan-$VERSION-arm64.dmg" \
                     "Fujisan (Apple Silicon)"
fi

if [[ -d "$PROJECT_ROOT/build-x86_64/Fujisan.app" ]]; then
    echo_info "Found x86_64 app bundle"
    create_dmg_safely "$PROJECT_ROOT/build-x86_64/Fujisan.app" \
                     "Fujisan-$VERSION-x86_64.dmg" \
                     "Fujisan (Intel)"
fi

echo_success "DMG creation complete!"
echo "Files in: $DIST_DIR"
ls -la "$DIST_DIR"/*.dmg 2>/dev/null || echo "No DMG files found"
