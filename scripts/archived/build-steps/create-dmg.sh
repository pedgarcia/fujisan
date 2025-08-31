#!/bin/bash
#
# create-dmg.sh - Create a professional DMG installer for Fujisan
#
# This script creates a properly formatted DMG with drag-to-Applications
# interface, custom background, and proper layout for distribution.
#

set -e

# Load configuration if not already loaded
if [[ -z "$PROJECT_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    source "$(dirname "$SCRIPT_DIR")/macos-config.sh"
fi

# Check prerequisites
if [[ ! -d "$BUILD_DIR/$APP_NAME" ]]; then
    echo_error "App bundle not found: $BUILD_DIR/$APP_NAME"
    exit 1
fi

echo_info "Creating DMG installer for Fujisan..."

cd "$BUILD_DIR"

# ============================================================================
# STEP 1: PREPARE DMG CONTENTS
# ============================================================================

DMG_TEMP_DIR="$TEMP_DIR/dmg-contents"
DMG_STAGING="$TEMP_DIR/dmg-staging.dmg"
DMG_FINAL="$DIST_DIR/$DMG_NAME"

echo_info "Preparing DMG contents in: $DMG_TEMP_DIR"

# Clean and create staging directory
rm -rf "$DMG_TEMP_DIR"
mkdir -p "$DMG_TEMP_DIR"

# Copy app bundle
echo_info "Copying app bundle..."
cp -R "$APP_NAME" "$DMG_TEMP_DIR/"

# Create Applications symlink
echo_info "Creating Applications symlink..."
ln -s /Applications "$DMG_TEMP_DIR/Applications"

# Copy additional files
if [[ -f "$PROJECT_ROOT/README.md" ]]; then
    echo_info "Adding README..."
    cp "$PROJECT_ROOT/README.md" "$DMG_TEMP_DIR/README.txt"
fi

if [[ -f "$PROJECT_ROOT/LICENSE" ]]; then
    echo_info "Adding LICENSE..."
    cp "$PROJECT_ROOT/LICENSE" "$DMG_TEMP_DIR/LICENSE.txt"
fi

# Add release notes if available
RELEASE_NOTES="$PROJECT_ROOT/RELEASE_NOTES.md"
if [[ -f "$RELEASE_NOTES" ]]; then
    echo_info "Adding release notes..."
    cp "$RELEASE_NOTES" "$DMG_TEMP_DIR/Release Notes.txt"
fi

# ============================================================================
# STEP 2: CALCULATE DMG SIZE
# ============================================================================

echo_info "Calculating DMG size..."

# Get size of contents
CONTENTS_SIZE=$(du -sm "$DMG_TEMP_DIR" | cut -f1)
# Add 50% padding for filesystem overhead
DMG_SIZE=$((CONTENTS_SIZE * 3 / 2))
# Minimum size of 100MB
if [[ $DMG_SIZE -lt 100 ]]; then
    DMG_SIZE=100
fi

echo_info "DMG size will be: ${DMG_SIZE}MB (contents: ${CONTENTS_SIZE}MB)"

# ============================================================================
# STEP 3: CREATE INITIAL DMG
# ============================================================================

echo_info "Creating initial DMG..."

# Create writable DMG
hdiutil create -srcfolder "$DMG_TEMP_DIR" \
               -volname "$DMG_TITLE" \
               -fs HFS+ \
               -fsargs "-c c=64,a=16,e=16" \
               -format UDRW \
               -size "${DMG_SIZE}m" \
               "$DMG_STAGING"

echo_info "✓ Initial DMG created"

# ============================================================================
# STEP 4: MOUNT AND CUSTOMIZE DMG
# ============================================================================

echo_info "Mounting DMG for customization..."

# Mount the DMG
MOUNT_POINT="/Volumes/$DMG_TITLE"
hdiutil attach "$DMG_STAGING" -readwrite -mount required

# Wait for mount
sleep 2

if [[ ! -d "$MOUNT_POINT" ]]; then
    echo_error "Failed to mount DMG at: $MOUNT_POINT"
    exit 1
fi

echo_info "DMG mounted at: $MOUNT_POINT"

# ============================================================================
# STEP 5: SET UP DMG LAYOUT AND APPEARANCE
# ============================================================================

echo_info "Customizing DMG appearance..."

# Parse window size
IFS=',' read -r WINDOW_WIDTH WINDOW_HEIGHT <<< "$DMG_WINDOW_SIZE"

# Parse positions
IFS=',' read -r APP_X APP_Y <<< "$DMG_APP_POSITION"
IFS=',' read -r APPS_X APPS_Y <<< "$DMG_APPLICATIONS_POSITION"

# Create AppleScript for DMG customization
APPLESCRIPT_FILE="$TEMP_DIR/dmg_setup.applescript"

cat > "$APPLESCRIPT_FILE" << EOF
tell application "Finder"
    tell disk "$DMG_TITLE"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {100, 100, $((100 + WINDOW_WIDTH)), $((100 + WINDOW_HEIGHT))}
        set theViewOptions to the icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to $DMG_ICON_SIZE
        
        -- Position items
        set position of item "$APP_NAME" of container window to {$APP_X, $APP_Y}
        set position of item "Applications" of container window to {$APPS_X, $APPS_Y}
        
        -- Set background if available
EOF

# Add background image if available
if [[ -f "$DMG_BACKGROUND" ]]; then
    echo_info "Setting custom background image..."
    
    # Copy background to DMG (hidden)
    mkdir -p "$MOUNT_POINT/.background"
    cp "$DMG_BACKGROUND" "$MOUNT_POINT/.background/background.png"
    
    cat >> "$APPLESCRIPT_FILE" << EOF
        set background picture of theViewOptions to file ".background:background.png"
EOF
else
    echo_info "No background image found, using default appearance"
fi

cat >> "$APPLESCRIPT_FILE" << EOF
        
        -- Update and close
        update without registering applications
        delay 2
        close
    end tell
end tell
EOF

# Run AppleScript to customize appearance
echo_info "Applying DMG layout..."
if osascript "$APPLESCRIPT_FILE"; then
    echo_info "✓ DMG layout applied successfully"
else
    echo_warn "⚠ DMG layout customization failed, but continuing..."
fi

# Ensure Finder has time to write changes
sleep 3

# ============================================================================
# STEP 6: FINALIZE DMG
# ============================================================================

echo_info "Finalizing DMG..."

# Unmount the DMG
hdiutil detach "$MOUNT_POINT" || {
    echo_warn "Failed to detach DMG cleanly, forcing detach..."
    hdiutil detach "$MOUNT_POINT" -force || true
}

# Convert to compressed read-only DMG
echo_info "Converting to final compressed DMG..."
hdiutil convert "$DMG_STAGING" \
                -format UDZO \
                -imagekey zlib-level=9 \
                -o "$DMG_FINAL"

# Clean up staging DMG
rm -f "$DMG_STAGING"

echo_info "✓ DMG conversion completed"

# ============================================================================
# STEP 7: SIGN DMG (if signing is enabled)
# ============================================================================

if [[ -n "$DEVELOPER_ID" && "$SKIP_SIGN" != "true" ]]; then
    echo_info "Signing DMG with Developer ID..."
    
    if codesign --sign "$DEVELOPER_ID" "$DMG_FINAL"; then
        echo_info "✓ DMG signed successfully"
        
        # Verify DMG signature
        if codesign --verify --verbose "$DMG_FINAL"; then
            echo_info "✓ DMG signature verified"
        else
            echo_warn "⚠ DMG signature verification failed"
        fi
    else
        echo_warn "⚠ Failed to sign DMG, but continuing..."
    fi
else
    echo_info "Skipping DMG signing (no signing identity or --skip-sign specified)"
fi

# ============================================================================
# STEP 8: VERIFICATION AND SUMMARY
# ============================================================================

echo_info "Verifying final DMG..."

# Check DMG integrity
if hdiutil verify "$DMG_FINAL"; then
    echo_info "✓ DMG integrity check passed"
else
    echo_error "DMG integrity check failed"
    exit 1
fi

# Get final file size
FINAL_SIZE=$(stat -f%z "$DMG_FINAL")
FINAL_SIZE_MB=$((FINAL_SIZE / 1024 / 1024))

echo_info "✓ DMG creation completed successfully"
echo_info "Final DMG: $DMG_FINAL"
echo_info "File size: ${FINAL_SIZE_MB}MB ($FINAL_SIZE bytes)"

# Test mount to ensure DMG works
echo_info "Testing DMG mount..."
TEST_MOUNT="/Volumes/Fujisan Test"
if hdiutil attach "$DMG_FINAL" -readonly -mount required; then
    sleep 2
    if [[ -d "$TEST_MOUNT" || -d "/Volumes/$DMG_TITLE" ]]; then
        echo_info "✓ DMG mounts successfully"
        # Unmount test
        hdiutil detach "/Volumes/$DMG_TITLE" 2>/dev/null || hdiutil detach "$TEST_MOUNT" 2>/dev/null || true
    else
        echo_warn "⚠ DMG test mount failed"
    fi
else
    echo_warn "⚠ DMG test mount failed"
fi

echo_info "DMG ready for distribution: $DMG_FINAL"