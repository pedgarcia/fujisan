#!/bin/bash
#
# build-macos-release.sh - Master script for building signed macOS Fujisan release
#
# This script orchestrates the complete process of building, signing, notarizing,
# and packaging Fujisan for macOS distribution.
#
# Usage: ./build-macos-release.sh [options]
#   --skip-build      Skip the build step (use existing build)
#   --skip-sign       Skip code signing step
#   --skip-notarize   Skip notarization step
#   --skip-dmg        Skip DMG creation step
#   --clean           Clean build directories before starting
#   --help            Show this help message
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Load configuration
source "$SCRIPT_DIR/macos-config.sh"

# ============================================================================
# COMMAND LINE ARGUMENT PARSING
# ============================================================================

SKIP_BUILD=false
SKIP_SIGN=false
SKIP_NOTARIZE=false
SKIP_DMG=false
CLEAN_BUILD=false

show_help() {
    cat << EOF
Fujisan macOS Release Builder

Usage: $0 [options]

Options:
  --skip-build      Skip the build step (use existing build)
  --skip-sign       Skip code signing step
  --skip-notarize   Skip notarization step
  --skip-dmg        Skip DMG creation step
  --clean           Clean build directories before starting
  --help            Show this help message

Environment Variables:
  DEVELOPER_ID          Apple Developer ID for code signing (optional)
  DEVELOPER_TEAM_ID     Apple Developer Team ID (optional)
  CMAKE_PREFIX_PATH     Path to Qt5 installation (auto-detected)
  FUJISAN_VERSION       Override version (default: from git)

Examples:
  # Full build process
  $0
  
  # Build without signing (for testing)
  $0 --skip-sign --skip-notarize
  
  # Clean build
  $0 --clean
  
  # Use existing build, just create DMG
  $0 --skip-build

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-sign)
            SKIP_SIGN=true
            shift
            ;;
        --skip-notarize)
            SKIP_NOTARIZE=true
            shift
            ;;
        --skip-dmg)
            SKIP_DMG=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# ============================================================================
# INITIALIZATION AND VALIDATION
# ============================================================================

echo_step "Starting Fujisan macOS Release Build"
echo_info "Project: $PROJECT_NAME v$PROJECT_VERSION"
echo_info "Bundle ID: $BUNDLE_ID"
echo_info "Build Dir: $BUILD_DIR"
echo_info "Output: $DMG_NAME"

# Create directories
mkdir -p "$BUILD_DIR" "$DIST_DIR" "$TEMP_DIR"

# Set up cleanup trap
trap cleanup_build EXIT

# Validate environment
if ! validate_environment; then
    echo_error "Environment validation failed. Please fix the issues above."
    exit 1
fi

# Clean build if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    echo_step "Cleaning build directories..."
    rm -rf "$BUILD_DIR"/*
    rm -rf "$DIST_DIR"/*
    echo_info "Build directories cleaned"
fi

# ============================================================================
# STEP 1: BUILD FUJISAN (includes libatari800 via CMake ExternalProject)
# ============================================================================

if [[ "$SKIP_BUILD" == "false" ]]; then
    echo_step "Building Fujisan (with embedded libatari800)..."
    
    cd "$BUILD_DIR"
    
    # Configure with CMake (libatari800 will be downloaded and built automatically)
    echo_info "Configuring CMake (this will download and build libatari800)..."
    cmake -G "$CMAKE_GENERATOR" \
          -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
          -DCMAKE_PREFIX_PATH="$QT5_PATH" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET" \
          "$PROJECT_ROOT"
    
    # Build (this will build libatari800 first, then Fujisan)
    echo_info "Building Fujisan (includes downloading/building libatari800)..."
    echo_info "This may take a few minutes on first build..."
    make -j"$BUILD_JOBS"
    
    if [[ ! -d "$BUILD_DIR/$APP_NAME" ]]; then
        echo_error "Build failed: $APP_NAME not found in $BUILD_DIR"
        exit 1
    fi
    
    echo_info "✓ Fujisan built successfully with embedded libatari800"
fi

# ============================================================================
# STEP 2: DEPLOY QT5 FRAMEWORKS
# ============================================================================

if [[ "$SKIP_BUILD" == "false" ]]; then
    echo_step "Deploying Qt5 frameworks..."
    
    cd "$BUILD_DIR"
    
    if [[ ! -f "$MACDEPLOYQT" ]]; then
        echo_error "macdeployqt not found at: $MACDEPLOYQT"
        exit 1
    fi
    
    echo_info "Running macdeployqt..."
    "$MACDEPLOYQT" "$APP_NAME" -verbose=2
    
    echo_info "✓ Qt5 frameworks deployed"
fi

# ============================================================================
# STEP 3: CODE SIGNING
# ============================================================================

if [[ "$SKIP_SIGN" == "false" && -n "$DEVELOPER_ID" ]]; then
    echo_step "Code signing application..."
    
    # Use helper script for signing
    if [[ -f "$SCRIPT_DIR/build-steps/sign-app.sh" ]]; then
        source "$SCRIPT_DIR/build-steps/sign-app.sh"
    else
        # Inline signing if helper script doesn't exist
        cd "$BUILD_DIR"
        
        echo_info "Signing with identity: $DEVELOPER_ID"
        
        # Sign frameworks first (deep signing)
        find "$APP_NAME/Contents/Frameworks" -name "*.framework" -type d | while read -r framework; do
            echo_info "Signing framework: $(basename "$framework")"
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$framework"
        done
        
        # Sign plugins
        find "$APP_NAME/Contents/PlugIns" -name "*.dylib" -type f 2>/dev/null | while read -r plugin; do
            echo_info "Signing plugin: $(basename "$plugin")"
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$plugin"
        done
        
        # Sign main executable
        echo_info "Signing main executable..."
        codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$APP_NAME/Contents/MacOS/$PROJECT_NAME"
        
        # Sign app bundle
        echo_info "Signing app bundle..."
        if [[ -f "$ENTITLEMENTS_FILE" ]]; then
            codesign $CODESIGN_OPTIONS --entitlements "$ENTITLEMENTS_FILE" --sign "$DEVELOPER_ID" "$APP_NAME"
        else
            codesign $CODESIGN_OPTIONS --sign "$DEVELOPER_ID" "$APP_NAME"
        fi
        
        # Verify signature
        echo_info "Verifying code signature..."
        codesign --verify --deep --strict --verbose=2 "$APP_NAME"
        spctl --assess --type execute --verbose "$APP_NAME"
    fi
    
    echo_info "✓ Code signing completed"
elif [[ "$SKIP_SIGN" == "true" ]]; then
    echo_warn "Skipping code signing (--skip-sign specified)"
    echo_info "Applying ad-hoc signature for execution..."
    cd "$BUILD_DIR"
    codesign --force --deep --sign - "$APP_NAME"
    echo_info "✓ Ad-hoc signature applied"
else
    echo_warn "Skipping code signing (no DEVELOPER_ID specified)"
    echo_info "Applying ad-hoc signature for execution..."
    cd "$BUILD_DIR"
    codesign --force --deep --sign - "$APP_NAME"
    echo_info "✓ Ad-hoc signature applied"
fi

# ============================================================================
# STEP 4: NOTARIZATION
# ============================================================================

if [[ "$SKIP_NOTARIZE" == "false" && "$SKIP_SIGN" == "false" && -n "$DEVELOPER_ID" ]]; then
    echo_step "Notarizing application..."
    
    # Use helper script for notarization
    if [[ -f "$SCRIPT_DIR/build-steps/notarize-app.sh" ]]; then
        source "$SCRIPT_DIR/build-steps/notarize-app.sh"
    else
        echo_warn "Notarization helper script not found. Skipping notarization."
        echo_info "To enable notarization, create: $SCRIPT_DIR/build-steps/notarize-app.sh"
    fi
elif [[ "$SKIP_NOTARIZE" == "true" ]]; then
    echo_warn "Skipping notarization (--skip-notarize specified)"
else
    echo_warn "Skipping notarization (signing was skipped or no DEVELOPER_ID)"
fi

# ============================================================================
# STEP 5: CREATE DMG
# ============================================================================

if [[ "$SKIP_DMG" == "false" ]]; then
    echo_step "Creating DMG..."
    
    # Use helper script for DMG creation
    if [[ -f "$SCRIPT_DIR/build-steps/create-dmg.sh" ]]; then
        source "$SCRIPT_DIR/build-steps/create-dmg.sh"
    else
        # Inline DMG creation if helper script doesn't exist
        cd "$BUILD_DIR"
        
        DMG_TEMP_DIR="$TEMP_DIR/dmg"
        mkdir -p "$DMG_TEMP_DIR"
        
        echo_info "Preparing DMG contents..."
        cp -R "$APP_NAME" "$DMG_TEMP_DIR/"
        ln -s /Applications "$DMG_TEMP_DIR/Applications"
        
        # Create DMG
        echo_info "Creating DMG: $DMG_NAME"
        hdiutil create -volname "$DMG_TITLE" \
                      -srcfolder "$DMG_TEMP_DIR" \
                      -ov -format UDZO \
                      "$DIST_DIR/$DMG_NAME"
        
        # Sign DMG if we have signing identity
        if [[ -n "$DEVELOPER_ID" && "$SKIP_SIGN" == "false" ]]; then
            echo_info "Signing DMG..."
            codesign --sign "$DEVELOPER_ID" "$DIST_DIR/$DMG_NAME"
        fi
    fi
    
    echo_info "✓ DMG created: $DIST_DIR/$DMG_NAME"
else
    echo_warn "Skipping DMG creation (--skip-dmg specified)"
fi

# ============================================================================
# COMPLETION
# ============================================================================

echo_step "Build completed successfully!"
echo_info "Project: $PROJECT_NAME v$PROJECT_VERSION"

if [[ -f "$DIST_DIR/$DMG_NAME" ]]; then
    DMG_SIZE=$(stat -f%z "$DIST_DIR/$DMG_NAME")
    echo_info "DMG: $DIST_DIR/$DMG_NAME ($DMG_SIZE bytes)"
fi

if [[ -d "$BUILD_DIR/$APP_NAME" ]]; then
    echo_info "App Bundle: $BUILD_DIR/$APP_NAME"
fi

echo_info "Build completed in: $BUILD_DIR"
echo_info "Distribution files in: $DIST_DIR"

# Generate checksums
if [[ -f "$DIST_DIR/$DMG_NAME" ]]; then
    echo_step "Generating checksums..."
    cd "$DIST_DIR"
    shasum -a 256 "$DMG_NAME" > "$DMG_NAME.sha256"
    echo_info "SHA256: $(cat "$DMG_NAME.sha256")"
fi

echo_info "✓ All done! Ready for distribution."