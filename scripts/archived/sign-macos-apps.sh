#!/bin/bash
#
# sign-macos-apps.sh - Sign Fujisan apps with Apple Developer certificate
#
# This script signs the apps with proper entitlements and Apple Developer ID
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"

# Signing identity - use the one we found
DEVELOPER_ID="Apple Development: Paulo Garcia (952JHD69PH)"

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

# Create entitlements file if it doesn't exist
ENTITLEMENTS="${SCRIPT_DIR}/fujisan.entitlements"
if [[ ! -f "$ENTITLEMENTS" ]]; then
    echo_info "Creating entitlements file..."
    cat > "$ENTITLEMENTS" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <!-- Allow JIT compilation for emulation -->
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    
    <!-- Allow unsigned executable memory for emulation -->
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    
    <!-- Allow DYLD environment variables -->
    <key>com.apple.security.cs.allow-dyld-environment-variables</key>
    <true/>
    
    <!-- Disable library validation to allow loading of Qt frameworks -->
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
    
    <!-- Disable executable page protection for emulation -->
    <key>com.apple.security.cs.disable-executable-page-protection</key>
    <true/>
    
    <!-- Audio input (microphone) - in case needed for cassette input -->
    <key>com.apple.security.device.audio-input</key>
    <true/>
    
    <!-- Network server for TCP server functionality -->
    <key>com.apple.security.network.server</key>
    <true/>
    
    <!-- Network client for NetSIO/FujiNet -->
    <key>com.apple.security.network.client</key>
    <true/>
    
    <!-- File access -->
    <key>com.apple.security.files.user-selected.read-write</key>
    <true/>
</dict>
</plist>
EOF
    echo_success "Created entitlements file"
fi

# Function to sign an app
sign_app() {
    local APP_PATH="$1"
    local ARCH_NAME="$2"
    
    if [[ ! -d "$APP_PATH" ]]; then
        echo_error "App not found at $APP_PATH"
        return 1
    fi
    
    echo_step "Signing $ARCH_NAME App"
    
    cd "$(dirname "$APP_PATH")"
    
    # First, remove any existing signatures
    echo_info "Removing existing signatures..."
    codesign --remove-signature "$(basename "$APP_PATH")" 2>/dev/null || true
    
    # Sign all frameworks first
    echo_info "Signing frameworks..."
    find "$(basename "$APP_PATH")/Contents/Frameworks" -name "*.framework" -type d 2>/dev/null | while read -r framework; do
        framework_name=$(basename "$framework" .framework)
        echo "  Signing $framework_name..."
        codesign --force --deep --sign "$DEVELOPER_ID" "$framework" 2>/dev/null || echo "    Warning: Could not sign $framework_name"
    done
    
    # Sign all dylibs
    echo_info "Signing libraries..."
    find "$(basename "$APP_PATH")/Contents" -name "*.dylib" -type f 2>/dev/null | while read -r dylib; do
        echo "  Signing $(basename "$dylib")..."
        codesign --force --sign "$DEVELOPER_ID" "$dylib" 2>/dev/null || echo "    Warning: Could not sign $(basename "$dylib")"
    done
    
    # Sign the main app bundle with entitlements
    echo_info "Signing main application..."
    if codesign --force --deep --sign "$DEVELOPER_ID" \
                --entitlements "$ENTITLEMENTS" \
                --options runtime \
                "$(basename "$APP_PATH")" 2>&1; then
        echo_success "App signed successfully"
    else
        echo_error "Failed to sign app"
        return 1
    fi
    
    # Verify the signature
    echo_info "Verifying signature..."
    if codesign --verify --deep --strict --verbose=2 "$(basename "$APP_PATH")" 2>&1; then
        echo_success "Signature verified"
    else
        echo_error "Signature verification failed"
    fi
    
    # Check gatekeeper
    echo_info "Checking Gatekeeper assessment..."
    if spctl --assess --type execute --verbose "$(basename "$APP_PATH")" 2>&1; then
        echo_success "Gatekeeper check passed"
    else
        echo_info "Gatekeeper check failed (this is normal for Development certificates)"
    fi
    
    # Display signature info
    echo_info "Signature details:"
    codesign -dv "$(basename "$APP_PATH")" 2>&1 | grep -E "Identifier|TeamIdentifier|Signature"
    
    echo ""
}

# Main execution
echo_step "macOS App Signing"
echo_info "Using identity: $DEVELOPER_ID"
echo ""

# Check if signing identity exists
if ! security find-identity -v -p codesigning | grep -q "$DEVELOPER_ID"; then
    echo_error "Signing identity not found: $DEVELOPER_ID"
    echo "Available identities:"
    security find-identity -v -p codesigning
    exit 1
fi

# Sign ARM64 app
if [[ -d "$ARM64_BUILD_DIR/Fujisan.app" ]]; then
    sign_app "$ARM64_BUILD_DIR/Fujisan.app" "ARM64"
else
    echo_info "ARM64 app not found, skipping"
fi

# Sign x86_64 app
if [[ -d "$X86_64_BUILD_DIR/Fujisan.app" ]]; then
    sign_app "$X86_64_BUILD_DIR/Fujisan.app" "x86_64"
else
    echo_info "x86_64 app not found, skipping"
fi

echo_step "Signing Complete"
echo ""
echo "Note: Apps signed with Development certificates will show warnings when"
echo "distributed to other users. For distribution, you need:"
echo "  • Developer ID Application certificate for direct distribution"
echo "  • Apple Distribution certificate for App Store"
echo ""
echo "The apps are now signed and ready for testing on your Mac!"