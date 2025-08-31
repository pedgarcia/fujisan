#!/bin/bash

# Quick fix for DMG creation issues
# This script helps with permission and mounting problems

# Function to safely create DMG
create_dmg_safe() {
    local APP_PATH="$1"
    local DMG_NAME="$2"
    local VOLUME_NAME="$3"
    local DIST_DIR="$4"
    
    echo "Creating DMG: $DMG_NAME"
    
    # Force unmount any existing volumes with the same name
    if [[ -d "/Volumes/$VOLUME_NAME" ]]; then
        echo "Force unmounting existing volume: $VOLUME_NAME"
        hdiutil detach "/Volumes/$VOLUME_NAME" -force 2>/dev/null || true
        sleep 2
    fi
    
    # Remove existing DMG file
    if [[ -f "$DIST_DIR/$DMG_NAME" ]]; then
        echo "Removing existing DMG: $DMG_NAME"
        rm -f "$DIST_DIR/$DMG_NAME"
    fi
    
    # Create temporary directory with unique name
    local TEMP_DIR=$(mktemp -d -t fujisan_dmg)
    echo "Using temporary directory: $TEMP_DIR"
    
    # Copy app bundle preserving all attributes
    echo "Copying app bundle..."
    cp -pR "$APP_PATH" "$TEMP_DIR/Fujisan.app"
    
    # Create Applications symlink
    echo "Creating Applications symlink..."
    ln -s /Applications "$TEMP_DIR/Applications"
    
    # Create simple README
    cat > "$TEMP_DIR/README.txt" << 'EOF'
Fujisan - Modern Atari Emulator
==============================

Installation:
1. Drag Fujisan.app to Applications folder
2. Double-click to launch

Visit: https://github.com/atari800/fujisan
EOF
    
    # Wait for filesystem operations to complete
    sleep 1
    
    # Create DMG with error handling
    echo "Creating DMG file..."
    if hdiutil create \
        -volname "$VOLUME_NAME" \
        -srcfolder "$TEMP_DIR" \
        -ov \
        -format UDZO \
        -imagekey zlib-level=9 \
        "$DIST_DIR/$DMG_NAME"; then
        echo "✓ DMG created successfully: $DMG_NAME"
    else
        echo "✗ Failed to create DMG"
        rm -rf "$TEMP_DIR"
        return 1
    fi
    
    # Clean up
    echo "Cleaning up..."
    rm -rf "$TEMP_DIR"
    
    # Generate checksum
    cd "$DIST_DIR" && shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
    echo "  SHA256: $(awk '{print $1}' "$DIST_DIR/${DMG_NAME}.sha256")"
    
    return 0
}

# Export the function so it can be used by other scripts
export -f create_dmg_safe

echo "DMG creation helper loaded. Use create_dmg_safe function."
