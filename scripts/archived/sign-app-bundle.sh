#!/bin/bash

# Fujisan App and DMG Signing Script
# This script properly signs the app bundle with hardened runtime before DMG creation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUNDLE_ID="com.atari.fujisan"
NOTARIZATION_PROFILE="fujisan-notarization"

# Parse command line options
DEVELOPER_ID=""
APP_PATH=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --developer-id)
            DEVELOPER_ID="$2"
            shift 2
            ;;
        --app-path)
            APP_PATH="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 --app-path /path/to/Fujisan.app [options]"
            echo "Options:"
            echo "  --app-path PATH      Path to Fujisan.app bundle (required)"
            echo "  --developer-id ID    Specify Developer ID (otherwise auto-detect)"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [[ -z "$APP_PATH" ]]; then
    echo "Error: --app-path is required"
    echo "Usage: $0 --app-path /path/to/Fujisan.app"
    exit 1
fi

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to find Developer ID
find_developer_id() {
    if [[ -n "$DEVELOPER_ID" ]]; then
        echo "$DEVELOPER_ID"
        return
    fi
    
    log_info "Looking for Developer ID Application certificate..."
    
    # Try to find Developer ID Application certificate
    local dev_id=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')
    
    if [[ -z "$dev_id" ]]; then
        log_error "No Developer ID Application certificate found"
        log_info "Available certificates:"
        security find-identity -v -p codesigning
        exit 1
    fi
    
    log_success "Found certificate: $dev_id"
    echo "$dev_id"
}

# Function to sign app bundle with hardened runtime
sign_app_bundle() {
    local app_path="$1"
    local dev_id="$2"
    
    if [[ ! -d "$app_path" ]]; then
        log_error "App bundle not found: $app_path"
        return 1
    fi
    
    log_info "Signing app bundle with hardened runtime: $(basename "$app_path")"
    
    # Create entitlements file for hardened runtime
    local entitlements_file="/tmp/fujisan_entitlements.plist"
    cat > "$entitlements_file" << EOF
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
    <key>com.apple.security.device.audio-input</key>
    <false/>
    <key>com.apple.security.device.camera</key>
    <false/>
</dict>
</plist>
EOF
    
    # Sign all frameworks and libraries first
    log_info "Signing frameworks and libraries..."
    
    # Sign Qt frameworks
    if [[ -d "$app_path/Contents/Frameworks" ]]; then
        find "$app_path/Contents/Frameworks" -name "*.dylib" -o -name "*.framework" | while read framework; do
            log_info "Signing: $(basename "$framework")"
            codesign --force --verify --verbose --timestamp \
                     --options runtime \
                     --sign "$dev_id" \
                     "$framework" 2>/dev/null || true
        done
    fi
    
    # Sign plugins
    if [[ -d "$app_path/Contents/PlugIns" ]]; then
        find "$app_path/Contents/PlugIns" -name "*.dylib" | while read plugin; do
            log_info "Signing: $(basename "$plugin")"
            codesign --force --verify --verbose --timestamp \
                     --options runtime \
                     --sign "$dev_id" \
                     "$plugin" 2>/dev/null || true
        done
    fi
    
    # Sign the main executable
    local executable="$app_path/Contents/MacOS/Fujisan"
    if [[ -f "$executable" ]]; then
        log_info "Signing main executable..."
        codesign --force --verify --verbose --timestamp \
                 --options runtime \
                 --entitlements "$entitlements_file" \
                 --sign "$dev_id" \
                 "$executable"
    fi
    
    # Sign the entire app bundle
    log_info "Signing app bundle..."
    codesign --force --verify --verbose --timestamp \
             --options runtime \
             --entitlements "$entitlements_file" \
             --sign "$dev_id" \
             "$app_path"
    
    # Clean up
    rm -f "$entitlements_file"
    
    # Verify the signature
    log_info "Verifying app bundle signature..."
    codesign --verify --deep --strict --verbose=2 "$app_path"
    
    # Check for hardened runtime
    log_info "Checking hardened runtime..."
    codesign --display --verbose=2 "$app_path" 2>&1 | grep -q "runtime" && \
        log_success "Hardened runtime enabled" || \
        log_error "Hardened runtime not detected"
    
    log_success "App bundle signed successfully: $(basename "$app_path")"
}

# Main execution
main() {
    log_info "Starting Fujisan app bundle signing process"
    
    # Find Developer ID
    local dev_id=$(find_developer_id)
    
    # Sign the app bundle
    sign_app_bundle "$APP_PATH" "$dev_id"
    
    log_success "App bundle signing completed!"
    log_info "The app is now ready for DMG creation and notarization"
    
    # Show final verification
    log_info "Final verification:"
    codesign --display --verbose=2 "$APP_PATH"
}

# Run main function
main
