#!/bin/bash
#
# notarize-app.sh - Notarize Fujisan app bundle with Apple
#
# This script submits the signed app to Apple's notarization service
# and waits for approval before stapling the notarization ticket.
#

set -e

# Load configuration if not already loaded
if [[ -z "$PROJECT_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    source "$(dirname "$SCRIPT_DIR")/macos-config.sh"
fi

# Check prerequisites
if [[ -z "$DEVELOPER_ID" ]]; then
    echo_error "DEVELOPER_ID not set. Cannot notarize application."
    exit 1
fi

if [[ ! -d "$BUILD_DIR/$APP_NAME" ]]; then
    echo_error "App bundle not found: $BUILD_DIR/$APP_NAME"
    exit 1
fi

echo_info "Notarizing Fujisan with Apple..."

cd "$BUILD_DIR"

# ============================================================================
# STEP 1: VALIDATE SIGNING
# ============================================================================

echo_info "Validating code signature before notarization..."

# Ensure the app is properly signed
if ! codesign --verify --deep --strict "$APP_NAME"; then
    echo_error "App bundle is not properly signed. Cannot proceed with notarization."
    exit 1
fi

# Check for hardened runtime
if ! codesign -dv --verbose=4 "$APP_NAME" 2>&1 | grep -q "flags=0x10000(runtime)"; then
    echo_error "App bundle is not signed with hardened runtime. Cannot proceed with notarization."
    exit 1
fi

echo_info "✓ Code signature validation passed"

# ============================================================================
# STEP 2: CREATE ZIP FOR NOTARIZATION
# ============================================================================

NOTARIZE_ZIP="$TEMP_DIR/${PROJECT_NAME}-notarize.zip"
echo_info "Creating zip archive for notarization: $NOTARIZE_ZIP"

# Create zip archive (required for notarization)
if ! ditto -c -k --keepParent "$APP_NAME" "$NOTARIZE_ZIP"; then
    echo_error "Failed to create zip archive for notarization"
    exit 1
fi

ZIP_SIZE=$(stat -f%z "$NOTARIZE_ZIP")
echo_info "Notarization archive created ($ZIP_SIZE bytes)"

# ============================================================================
# STEP 3: SUBMIT FOR NOTARIZATION
# ============================================================================

echo_info "Submitting to Apple notarization service..."

# Check for notarization credentials
if ! xcrun notarytool store-credentials --list | grep -q "$NOTARIZATION_PROFILE"; then
    echo_error "Notarization profile '$NOTARIZATION_PROFILE' not found."
    echo_info "Please create a notarization profile with:"
    echo_info "  xcrun notarytool store-credentials --apple-id <your-apple-id> --team-id $DEVELOPER_TEAM_ID --password <app-specific-password> $NOTARIZATION_PROFILE"
    exit 1
fi

# Submit for notarization
echo_info "Submitting $APP_NAME for notarization..."
SUBMIT_OUTPUT=$(xcrun notarytool submit "$NOTARIZE_ZIP" \
                                      --keychain-profile "$NOTARIZATION_PROFILE" \
                                      --wait \
                                      --timeout "${NOTARIZATION_TIMEOUT}s" 2>&1)

# Check submission result
if echo "$SUBMIT_OUTPUT" | grep -q "status: Accepted"; then
    echo_info "✓ Notarization successful!"
    SUBMISSION_ID=$(echo "$SUBMIT_OUTPUT" | grep "id:" | head -1 | awk '{print $2}')
    echo_info "Submission ID: $SUBMISSION_ID"
else
    echo_error "Notarization failed!"
    echo "$SUBMIT_OUTPUT"
    
    # Try to get more details about the failure
    if echo "$SUBMIT_OUTPUT" | grep -q "id:"; then
        SUBMISSION_ID=$(echo "$SUBMIT_OUTPUT" | grep "id:" | head -1 | awk '{print $2}')
        echo_info "Getting detailed failure information..."
        xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "$NOTARIZATION_PROFILE"
    fi
    
    exit 1
fi

# ============================================================================
# STEP 4: STAPLE NOTARIZATION TICKET
# ============================================================================

echo_info "Stapling notarization ticket to app bundle..."

if ! xcrun stapler staple "$APP_NAME"; then
    echo_error "Failed to staple notarization ticket"
    exit 1
fi

echo_info "✓ Notarization ticket stapled successfully"

# ============================================================================
# STEP 5: FINAL VERIFICATION
# ============================================================================

echo_info "Performing final verification..."

# Verify stapling
if ! xcrun stapler validate "$APP_NAME"; then
    echo_error "Stapled notarization ticket validation failed"
    exit 1
fi

# Final Gatekeeper check
if spctl --assess --type execute --verbose "$APP_NAME"; then
    echo_info "✓ Final Gatekeeper assessment passed"
else
    echo_error "Final Gatekeeper assessment failed"
    exit 1
fi

# Display notarization info
echo_info "Notarization information:"
if command -v stapler >/dev/null 2>&1; then
    stapler validate "$APP_NAME" -v 2>&1 | head -5 || true
fi

# Cleanup
rm -f "$NOTARIZE_ZIP"

echo_info "✓ Notarization completed successfully"
echo_info "App bundle is ready for distribution: $APP_NAME"