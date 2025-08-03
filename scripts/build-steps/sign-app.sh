#!/bin/bash
#
# sign-app.sh - Code sign Fujisan app bundle for macOS distribution
#
# This script handles the complete code signing process including frameworks,
# plugins, executable, and app bundle with proper hardened runtime.
#

set -e

# Load configuration if not already loaded
if [[ -z "$PROJECT_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    source "$(dirname "$SCRIPT_DIR")/macos-config.sh"
fi

# Check prerequisites
if [[ -z "$DEVELOPER_ID" ]]; then
    echo_error "DEVELOPER_ID not set. Cannot sign application."
    echo_info "Please set DEVELOPER_ID environment variable with your signing identity."
    exit 1
fi

if [[ ! -d "$BUILD_DIR/$APP_NAME" ]]; then
    echo_error "App bundle not found: $BUILD_DIR/$APP_NAME"
    exit 1
fi

echo_info "Code signing Fujisan with identity: $DEVELOPER_ID"

cd "$BUILD_DIR"

# ============================================================================
# STEP 1: SIGN EMBEDDED FRAMEWORKS
# ============================================================================

echo_info "Signing Qt5 frameworks..."

FRAMEWORKS_DIR="$APP_NAME/Contents/Frameworks"
if [[ -d "$FRAMEWORKS_DIR" ]]; then
    # Find and sign all frameworks
    find "$FRAMEWORKS_DIR" -name "*.framework" -type d | while read -r framework; do
        framework_name=$(basename "$framework" .framework)
        echo_info "  Signing framework: $framework_name"
        
        # Sign the framework binary if it exists
        if [[ -f "$framework/Versions/Current/$framework_name" ]]; then
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$framework/Versions/Current/$framework_name"
        elif [[ -f "$framework/$framework_name" ]]; then
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$framework/$framework_name"
        fi
        
        # Sign the framework bundle
        codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$framework"
    done
    
    # Sign any standalone dylibs in Frameworks
    find "$FRAMEWORKS_DIR" -name "*.dylib" -type f | while read -r dylib; do
        echo_info "  Signing library: $(basename "$dylib")"
        codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$dylib"
    done
else
    echo_warn "No Frameworks directory found, skipping framework signing"
fi

# ============================================================================
# STEP 2: SIGN PLUGINS
# ============================================================================

echo_info "Signing Qt5 plugins..."

PLUGINS_DIR="$APP_NAME/Contents/PlugIns"
if [[ -d "$PLUGINS_DIR" ]]; then
    find "$PLUGINS_DIR" -name "*.dylib" -type f | while read -r plugin; do
        plugin_name=$(basename "$plugin")
        echo_info "  Signing plugin: $plugin_name"
        codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$plugin"
    done
else
    echo_warn "No PlugIns directory found, skipping plugin signing"
fi

# ============================================================================
# STEP 3: SIGN HELPER EXECUTABLES
# ============================================================================

# Sign any helper executables in MacOS directory
MACOS_DIR="$APP_NAME/Contents/MacOS"
if [[ -d "$MACOS_DIR" ]]; then
    find "$MACOS_DIR" -type f -perm +111 | while read -r executable; do
        if [[ "$(basename "$executable")" != "$PROJECT_NAME" ]]; then
            echo_info "  Signing helper executable: $(basename "$executable")"
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$executable"
        fi
    done
fi

# ============================================================================
# STEP 4: SIGN MAIN EXECUTABLE
# ============================================================================

MAIN_EXECUTABLE="$APP_NAME/Contents/MacOS/$PROJECT_NAME"
if [[ -f "$MAIN_EXECUTABLE" ]]; then
    echo_info "Signing main executable: $PROJECT_NAME"
    codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$MAIN_EXECUTABLE"
else
    echo_error "Main executable not found: $MAIN_EXECUTABLE"
    exit 1
fi

# ============================================================================
# STEP 5: SIGN APP BUNDLE
# ============================================================================

echo_info "Signing app bundle with hardened runtime..."

# Prepare signing command with entitlements if available
SIGN_CMD="codesign $CODESIGN_OPTIONS"

if [[ -f "$ENTITLEMENTS_FILE" ]]; then
    echo_info "Using entitlements file: $ENTITLEMENTS_FILE"
    SIGN_CMD="$SIGN_CMD --entitlements $ENTITLEMENTS_FILE"
else
    echo_warn "No entitlements file found, signing without entitlements"
fi

SIGN_CMD="$SIGN_CMD --sign $DEVELOPER_ID $APP_NAME"

if ! eval "$SIGN_CMD"; then
    echo_error "Failed to sign app bundle"
    exit 1
fi

# ============================================================================
# STEP 6: VERIFICATION
# ============================================================================

echo_info "Verifying code signature..."

# Verify the signature
if ! codesign --verify --deep --strict --verbose=2 "$APP_NAME"; then
    echo_error "Code signature verification failed"
    exit 1
fi

# Check with spctl (Gatekeeper assessment)
echo_info "Checking Gatekeeper assessment..."
if spctl --assess --type execute --verbose "$APP_NAME"; then
    echo_info "✓ Gatekeeper assessment passed"
else
    echo_warn "⚠ Gatekeeper assessment failed (may need notarization)"
fi

# Display signature information
echo_info "Code signature details:"
codesign -dv --verbose=4 "$APP_NAME" 2>&1 | grep -E "(Identifier|TeamIdentifier|Timestamp|Runtime Version)" || true

echo_info "✓ Code signing completed successfully"
echo_info "App bundle signed with Developer ID: $DEVELOPER_ID"