#!/bin/bash
#
# build.sh - Universal build script for Fujisan
#
# Usage: ./build.sh [platform] [options]
#
# Platforms:
#   macos-arm64    - Build for Apple Silicon Macs
#   macos-x86_64   - Build for Intel Macs
#   macos          - Build for both Mac architectures
#   windows        - Cross-compile for Windows
#   linux          - Build for Linux x86_64 (using Docker/Podman)
#   linux-x86_64   - Build for Linux x86_64 explicitly
#   linux-amd64    - Build for Linux x86_64 (alias)
#   linux-arm64    - Build for Linux ARM64/aarch64
#   linux-aarch64  - Build for Linux ARM64 (alias)
#   all            - Build for all platforms
#
# Options:
#   --clean        - Clean before building
#   --sign         - Sign macOS builds (requires certificates)
#   --notarize     - Notarize macOS builds after signing
#   --developer-id - Specify Developer ID for signing
#   --version X    - Set version (default: from git)
#   --help         - Show this help
#
# All outputs go to: dist/
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Output directories for each platform
DIST_DIR="${PROJECT_ROOT}/dist"
MACOS_DIST_DIR="${DIST_DIR}/macos"
WINDOWS_DIST_DIR="${DIST_DIR}/windows"
LINUX_DIST_DIR="${DIST_DIR}/linux"

# macOS Build directories
ARM64_BUILD_DIR="${PROJECT_ROOT}/build-arm64"
X86_64_BUILD_DIR="${PROJECT_ROOT}/build-x86_64"

# Qt paths for macOS
QT_ARM64_PATH="/opt/homebrew/opt/qt@5"
QT_X86_64_PATH="/usr/local/opt/qt@5"

# Version
# Full version from git (e.g., "v1.0.5-9-g296092d")
VERSION="${VERSION:-$(git describe --tags --always 2>/dev/null || echo "v1.0.0-dev")}"
# Remove 'v' prefix (e.g., "1.0.5-9-g296092d")
# Note: CMakeLists.txt will extract numeric version for CMake's project(VERSION)
VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')

# Notarization settings
BUNDLE_ID="com.atari.fujisan"
NOTARIZATION_PROFILE="fujisan-notarization"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo_error() { echo -e "${RED}ERROR: $1${NC}" >&2; }
echo_success() { echo -e "${GREEN}✓ $1${NC}"; }
echo_info() { echo -e "${YELLOW}→ $1${NC}"; }
echo_step() { echo -e "${BLUE}=== $1 ===${NC}"; }

# Show help
show_help() {
    cat << EOF
Fujisan Universal Build Script

Usage: $0 [platform] [options]

Platforms:
  macos-arm64    Build for Apple Silicon Macs
  macos-x86_64   Build for Intel Macs
  macos          Build for both Mac architectures
  windows        Cross-compile for Windows
  linux          Build for Linux x86_64 (Docker/Podman)
  linux-x86_64   Build for Linux x86_64 explicitly
  linux-amd64    Build for Linux x86_64 (alias)
  linux-arm64    Build for Linux ARM64/aarch64
  linux-aarch64  Build for Linux ARM64 (alias)
  all            Build for all platforms

Options:
  --clean        Clean before building
  --sign         Sign macOS builds with Developer ID
  --notarize     Notarize macOS builds (implies --sign)
  --developer-id ID  Specify Developer ID certificate
  --version X    Set version
  --help         Show this help

Examples:
  $0 macos                    # Build both Mac versions
  $0 macos --sign --notarize   # Build, sign, and notarize
  $0 windows --clean           # Clean build for Windows
  $0 all --version v1.2.0      # Build all platforms

Output:
  All builds output to: dist/
  
  dist/macos/
    - Fujisan-{version}-arm64.dmg
    - Fujisan-{version}-x86_64.dmg
  dist/windows/
    - Fujisan-{version}-windows.zip
  dist/linux/
    - fujisan-{version}-linux-x64.tar.gz (x86_64)
    - fujisan_{version}_amd64.deb (x86_64)
    - fujisan-{version}-linux-arm64.tar.gz (ARM64)
    - fujisan_{version}_arm64.deb (ARM64)

EOF
}

# Clean function
clean_all() {
    echo_info "Cleaning all build directories..."
    rm -rf build/ build-*/ dist/*
    echo_success "Clean complete"
}

# Function to find Developer ID certificate
find_developer_id() {
    if [[ -n "$DEVELOPER_ID" ]]; then
        echo "$DEVELOPER_ID"
        return
    fi
    
    echo_info "Looking for Developer ID Application certificate..."
    
    # Get the actual certificate hash (40-character hex string)
    local cert_hash=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}')
    
    if [[ -z "$cert_hash" ]] || [[ "$cert_hash" == "" ]]; then
        echo_error "No Developer ID Application certificate found"
        echo_info "Available certificates:"
        security find-identity -v -p codesigning
        echo_info "Install a Developer ID Application certificate or run without --sign"
        exit 1
    fi
    
    # Get the friendly name for display
    local cert_name=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')
    
    echo_success "Found certificate: $cert_name"
    echo_info "Using certificate hash: $cert_hash"
    
    # Return the hash (which codesign prefers)
    echo "$cert_hash"
}

# Function to sign app bundle with hardened runtime
sign_app_bundle() {
    local app_path="$1"
    local dev_id="$2"
    
    echo_info "Signing app bundle with hardened runtime: $(basename "$app_path")"
    
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
    
    # Sign all frameworks and libraries first
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
    
    # Sign the main executable
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path/Contents/MacOS/Fujisan"
    
    # Sign the entire app bundle
    codesign --force --timestamp --options runtime --entitlements "$entitlements" --sign "$dev_id" "$app_path"
    
    # Clean up
    rm -f "$entitlements"
    
    # Verify the signature
    codesign --verify --deep --strict --verbose=2 "$app_path"
    
    # Check for hardened runtime
    if codesign --display --verbose=2 "$app_path" 2>&1 | grep -q "runtime"; then
        echo_success "Hardened runtime enabled"
    else
        echo_error "Hardened runtime not detected"
    fi
    
    echo_success "App bundle signed: $(basename "$app_path")"
}

# Function to create DMG
create_dmg() {
    local APP_PATH="$1"
    local DMG_NAME="$2"
    local VOLUME_NAME="$3"
    local SIGN_DMG="$4"
    local DEV_ID="$5"
    
    if [[ ! -d "$APP_PATH" ]]; then
        echo_error "App not found at $APP_PATH"
        return 1
    fi
    
    echo_info "Creating DMG: $DMG_NAME"
    
    # Ensure output directory exists
    mkdir -p "$MACOS_DIST_DIR"
    
    # Create temporary directory for DMG
    local TEMP_DIR=$(mktemp -d)
    
    # Copy app to temp directory
    cp -R "$APP_PATH" "$TEMP_DIR/Fujisan.app"
    
    # Create Applications symlink
    ln -s /Applications "$TEMP_DIR/Applications"
    
    # Create README if desired
    cat > "$TEMP_DIR/README.txt" << EOF
Fujisan v${VERSION}
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
                   "$MACOS_DIST_DIR/$DMG_NAME"
    
    # Clean up
    rm -rf "$TEMP_DIR"
    
    # Sign the DMG if requested
    if [[ "$SIGN_DMG" == "true" ]] && [[ -n "$DEV_ID" ]]; then
        echo_info "Signing DMG..."
        codesign --force --verify --verbose --sign "$DEV_ID" "$MACOS_DIST_DIR/$DMG_NAME"
        codesign --verify --deep --strict --verbose=2 "$MACOS_DIST_DIR/$DMG_NAME"
        echo_success "DMG signed"
    fi
    
    echo_success "Created: $MACOS_DIST_DIR/$DMG_NAME"
    
    # Generate checksum
    cd "$MACOS_DIST_DIR"
    shasum -a 256 "$DMG_NAME" > "${DMG_NAME}.sha256"
    echo "  SHA256: $(awk '{print $1}' "${DMG_NAME}.sha256")"
    cd - > /dev/null
}

# Function to bundle FujiNet-PC binary
bundle_fujinet_pc() {
    local app_path="$1"
    local arch="$2"  # "arm64" or "x86_64"
    local dev_id="$3"  # Optional: Developer ID for signing

    echo_info "Bundling FujiNet-PC binary for $arch..."

    # Use local pre-built FujiNet-PC binary from fujinet-firmware
    local FUJINET_SOURCE_DIR="${PROJECT_ROOT}/../fujinet-firmware/build/dist"

    if [[ ! -f "$FUJINET_SOURCE_DIR/fujinet" ]]; then
        echo_error "FujiNet-PC binary not found at: $FUJINET_SOURCE_DIR/fujinet"
        echo_info "Expected location: $FUJINET_SOURCE_DIR"
        echo_info "Please build FujiNet-PC first: cd ${PROJECT_ROOT}/../fujinet-firmware && ./build.sh"
        return 1
    fi

    # Get version from binary
    local version=$("$FUJINET_SOURCE_DIR/fujinet" -V 2>&1 | grep "^FujiNet-PC" | awk '{print $2}')
    if [[ -z "$version" ]]; then
        version="local-build"
    fi

    echo_info "Found FujiNet-PC $version at: $FUJINET_SOURCE_DIR"

    # Create Resources/fujinet-pc directory in app bundle
    local fujinet_dir="$app_path/Contents/Resources/fujinet-pc"
    rm -rf "$fujinet_dir"  # Clean slate
    mkdir -p "$fujinet_dir"

    # Copy binary
    cp "$FUJINET_SOURCE_DIR/fujinet" "$fujinet_dir/fujinet"
    chmod +x "$fujinet_dir/fujinet"

    # Copy data folder (required for handlers and printer fonts)
    if [[ -d "$FUJINET_SOURCE_DIR/data" ]]; then
        echo_info "Copying data folder..."
        cp -R "$FUJINET_SOURCE_DIR/data" "$fujinet_dir/"
    else
        echo_warning "data folder not found - FujiNet may not function properly"
    fi

    # Create SD folder for SD card emulation
    mkdir -p "$fujinet_dir/SD"
    echo "--- FujiNet SD Card ---" > "$fujinet_dir/SD/README.txt"
    echo "Place disk images (.atr, .xex files) here for FujiNet access" >> "$fujinet_dir/SD/README.txt"

    # Create default fnconfig.ini
    cat > "$fujinet_dir/fnconfig.ini" << 'EOF'
[General]
devicename=
hsioindex=8
rotationsounds=1
configenabled=1
config_ng=0
altconfigfile=
boot_mode=0
fnconfig_on_spifs=1
status_wait_enabled=1
printer_enabled=1
encrypt_passphrase=0

[WiFi]
enabled=1
SSID=Dummy Cafe
passphrase=

[Bluetooth]
devicename=SIO2BTFujiNet
enabled=0
baud=19200

[Network]
sntpserver=pool.ntp.org

[Host1]
type=SD
name=SD

[Host2]
type=TNFS
name=fujinet.online

[Host3]
type=TNFS
name=fujinet.pl

[Modem]
modem_enabled=1
sniffer_enabled=0

[Cassette]
play_record=0 Play
pulldown=1 Pulldown Resistor
cassette_enabled=1

[CPM]
cpm_enabled=1
ccp=

[ENABLE]
enable_device_slot_1=1
enable_device_slot_2=1
enable_device_slot_3=1
enable_device_slot_4=1
enable_device_slot_5=1
enable_device_slot_6=1
enable_device_slot_7=1
enable_device_slot_8=1
enable_apetime=1
enable_pclink=1

[BOIP]
enabled=1
host=localhost
port=

[Serial]
port=
command=DSR
proceed=DTR
EOF

    echo_info "Created default fnconfig.ini"

    # Create version file
    echo "$version" > "$fujinet_dir/version.txt"

    # Sign the binary if Developer ID provided
    if [[ -n "$dev_id" ]]; then
        echo_info "Signing FujiNet-PC binary..."
        codesign --force --timestamp --options runtime --sign "$dev_id" "$fujinet_dir/fujinet"
        echo_success "FujiNet-PC binary signed"
    fi

    echo_success "FujiNet-PC $version bundled successfully"
}

# Function to notarize DMG
notarize_dmg() {
    local dmg_path="$1"
    
    if [[ ! -f "$dmg_path" ]]; then
        echo_error "DMG not found: $dmg_path"
        return 1
    fi
    
    echo_info "Submitting for notarization: $(basename "$dmg_path")"
    
    # Check if notarization profile exists
    if ! xcrun notarytool history --keychain-profile "$NOTARIZATION_PROFILE" &>/dev/null; then
        echo_error "Notarization profile '$NOTARIZATION_PROFILE' not found"
        echo_info "To create it, run:"
        echo_info "xcrun notarytool store-credentials \"$NOTARIZATION_PROFILE\" \\"
        echo_info "    --apple-id \"your-apple-id@example.com\" \\"
        echo_info "    --team-id \"YOUR_TEAM_ID\" \\"
        echo_info "    --password \"app-specific-password\""
        return 1
    fi
    
    # Submit for notarization
    local submission_output=$(xcrun notarytool submit "$dmg_path" \
        --keychain-profile "$NOTARIZATION_PROFILE" \
        --wait --output-format json 2>&1)
    
    local submission_id=$(echo "$submission_output" | jq -r '.id' 2>/dev/null)
    
    if [[ "$submission_id" == "null" || -z "$submission_id" ]]; then
        echo_error "Failed to submit for notarization"
        echo "$submission_output"
        return 1
    fi
    
    echo_info "Submission ID: $submission_id"
    
    # Check status
    local status=$(echo "$submission_output" | jq -r '.status' 2>/dev/null)
    
    if [[ "$status" == "Accepted" ]]; then
        echo_success "Notarization successful!"
        
        # Staple the notarization
        echo_info "Stapling notarization to DMG..."
        xcrun stapler staple "$dmg_path"
        
        # Verify stapling
        echo_info "Verifying stapled notarization..."
        xcrun stapler validate "$dmg_path"
        
        echo_success "DMG notarized and stapled: $(basename "$dmg_path")"
    else
        echo_error "Notarization failed with status: $status"
        
        # Get detailed log
        echo_info "Fetching notarization log..."
        xcrun notarytool log "$submission_id" \
            --keychain-profile "$NOTARIZATION_PROFILE"
        
        return 1
    fi
}

# Build macOS ARM64
build_macos_arm64() {
    echo_step "Building macOS ARM64"
    
    if [[ ! -d "$QT_ARM64_PATH" ]]; then
        echo_error "Qt5 for ARM64 not found. Install with: brew install qt@5"
        return 1
    fi
    
    # Create build directory if it doesn't exist
    mkdir -p "$ARM64_BUILD_DIR"
    cd "$ARM64_BUILD_DIR"
    
    # Build if needed
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]] || [[ "$CLEAN" == "true" ]]; then
        echo_info "Configuring with CMake..."
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=arm64 \
              -DCMAKE_PREFIX_PATH="$QT_ARM64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              -DPROJECT_VERSION="$VERSION_CLEAN" \
              "$PROJECT_ROOT"
        
        echo_info "Building..."
        make -j$(sysctl -n hw.ncpu)
    else
        echo_info "Using existing ARM64 build"
    fi
    
    # Deploy Qt frameworks
    echo_info "Deploying Qt frameworks..."
    "$QT_ARM64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Copy images to app bundle
    echo_info "Copying images to app bundle..."
    mkdir -p "Fujisan.app/Contents/Resources/images"
    cp -r "$PROJECT_ROOT/images/"*.png "Fujisan.app/Contents/Resources/images/" 2>/dev/null || true
    
    # Sign with Developer ID or ad-hoc
    local dev_id=""
    if [[ "$SIGN" == "true" ]] || [[ "$NOTARIZE" == "true" ]]; then
        dev_id=$(find_developer_id)
        sign_app_bundle "Fujisan.app" "$dev_id"
    else
        echo_info "Ad-hoc signing app..."
        codesign --force --deep --sign - "Fujisan.app"
    fi

    # Bundle FujiNet-PC if requested
    if [[ "$BUNDLE_FUJINET" == "true" ]]; then
        bundle_fujinet_pc "$ARM64_BUILD_DIR/Fujisan.app" "arm64" "$dev_id"
    fi

    # Verify architecture
    echo_info "Verifying ARM64 build..."
    EXEC_ARCH=$(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")
    if [[ "$EXEC_ARCH" == "arm64" ]]; then
        echo_success "ARM64 executable confirmed"
    else
        echo_error "Unexpected architecture: $EXEC_ARCH"
    fi

    # Create DMG
    local dmg_sign="false"
    if [[ "$SIGN" == "true" ]] || [[ "$NOTARIZE" == "true" ]]; then
        dmg_sign="true"
    fi

    create_dmg "$ARM64_BUILD_DIR/Fujisan.app" \
               "Fujisan-${VERSION_CLEAN}-arm64.dmg" \
               "Fujisan (Apple Silicon)" \
               "$dmg_sign" \
               "$dev_id"
    
    # Notarize if requested
    if [[ "$NOTARIZE" == "true" ]]; then
        notarize_dmg "$MACOS_DIST_DIR/Fujisan-${VERSION_CLEAN}-arm64.dmg"
    fi
    
    # Return to project root
    cd "$PROJECT_ROOT"
    
    echo_success "macOS ARM64 build complete"
}

# Build macOS x86_64
build_macos_x86_64() {
    echo_step "Building macOS x86_64"
    
    if [[ ! -d "$QT_X86_64_PATH" ]]; then
        echo_error "Qt5 for x86_64 not found. Install with: arch -x86_64 /usr/local/bin/brew install qt@5"
        return 1
    fi
    
    # Create build directory if it doesn't exist
    mkdir -p "$X86_64_BUILD_DIR"
    cd "$X86_64_BUILD_DIR"
    
    # Build if needed
    if [[ ! -f "Fujisan.app/Contents/MacOS/Fujisan" ]] || [[ "$CLEAN" == "true" ]]; then
        echo_info "Configuring with CMake..."
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_OSX_ARCHITECTURES=x86_64 \
              -DCMAKE_PREFIX_PATH="$QT_X86_64_PATH" \
              -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE \
              -DPROJECT_VERSION="$VERSION_CLEAN" \
              "$PROJECT_ROOT"
        
        echo_info "Building..."
        make -j$(sysctl -n hw.ncpu)
    else
        echo_info "Using existing x86_64 build"
    fi
    
    # Deploy Qt frameworks
    echo_info "Deploying Qt frameworks..."
    "$QT_X86_64_PATH/bin/macdeployqt" "Fujisan.app"
    
    # Copy images to app bundle
    echo_info "Copying images to app bundle..."
    mkdir -p "Fujisan.app/Contents/Resources/images"
    cp -r "$PROJECT_ROOT/images/"*.png "Fujisan.app/Contents/Resources/images/" 2>/dev/null || true
    
    # Sign with Developer ID or ad-hoc
    local dev_id=""
    if [[ "$SIGN" == "true" ]] || [[ "$NOTARIZE" == "true" ]]; then
        dev_id=$(find_developer_id)
        sign_app_bundle "Fujisan.app" "$dev_id"
    else
        echo_info "Ad-hoc signing app..."
        codesign --force --deep --sign - "Fujisan.app"
    fi

    # Bundle FujiNet-PC if requested
    if [[ "$BUNDLE_FUJINET" == "true" ]]; then
        bundle_fujinet_pc "$X86_64_BUILD_DIR/Fujisan.app" "x86_64" "$dev_id"
    fi

    # Verify architecture
    echo_info "Verifying x86_64 build..."
    EXEC_ARCH=$(lipo -archs "Fujisan.app/Contents/MacOS/Fujisan")
    if [[ "$EXEC_ARCH" == "x86_64" ]]; then
        echo_success "x86_64 executable confirmed"
    else
        echo_error "Unexpected architecture: $EXEC_ARCH"
    fi
    
    # Create DMG
    local dmg_sign="false"
    if [[ "$SIGN" == "true" ]] || [[ "$NOTARIZE" == "true" ]]; then
        dmg_sign="true"
    fi
    
    create_dmg "$X86_64_BUILD_DIR/Fujisan.app" \
               "Fujisan-${VERSION_CLEAN}-x86_64.dmg" \
               "Fujisan (Intel)" \
               "$dmg_sign" \
               "$dev_id"
    
    # Notarize if requested
    if [[ "$NOTARIZE" == "true" ]]; then
        notarize_dmg "$MACOS_DIST_DIR/Fujisan-${VERSION_CLEAN}-x86_64.dmg"
    fi
    
    # Return to project root
    cd "$PROJECT_ROOT"
    
    echo_success "macOS x86_64 build complete"
}

# Build Windows
build_windows() {
    echo_step "Building Windows"
    echo_info "Starting Windows build function..."
    echo_info "WINDOWS_DIST_DIR: $WINDOWS_DIST_DIR"
    echo_info "VERSION_CLEAN: $VERSION_CLEAN"
    
    # Ensure we're in the project root
    cd "$PROJECT_ROOT"
    echo_info "Current directory: $(pwd)"
    
    # Check for Docker/Podman
    if command -v podman &> /dev/null; then
        CONTAINER_RUNTIME="podman"
        echo_info "Using podman for Windows build"
    elif command -v docker &> /dev/null; then
        CONTAINER_RUNTIME="docker"
        echo_info "Using docker for Windows build"
    else
        echo_error "Neither podman nor docker found. Install one to build for Windows."
        return 1
    fi
    
    # Clean Windows build directory
    rm -rf build-cross-windows/
    
    # Use the simplified Windows build script
    if [[ -f "$SCRIPT_DIR/scripts/build-windows-simple.sh" ]]; then
        echo_info "Passing VERSION=$VERSION to Windows build script"
        VERSION="$VERSION" "$SCRIPT_DIR/scripts/build-windows-simple.sh"
        
        # Package into zip for distribution
        if [[ -d "build-windows" ]]; then
            echo_info "Creating Windows distribution package..."
            echo_info "Windows build directory contains:"
            ls -la build-windows/ | head -10
            
            mkdir -p "$WINDOWS_DIST_DIR"
            echo_info "Creating zip in: $WINDOWS_DIST_DIR/Fujisan-${VERSION_CLEAN}-windows.zip"
            
            cd build-windows
            if zip -r "$WINDOWS_DIST_DIR/Fujisan-${VERSION_CLEAN}-windows.zip" * -x "*.log"; then
                cd ..
                echo_success "Created: dist/windows/Fujisan-${VERSION_CLEAN}-windows.zip"
            else
                cd ..
                echo_error "Failed to create zip file"
                return 1
            fi
            
            # Verify the zip was created
            if [[ -f "$WINDOWS_DIST_DIR/Fujisan-${VERSION_CLEAN}-windows.zip" ]]; then
                # Generate checksum
                cd "$WINDOWS_DIST_DIR"
                shasum -a 256 "Fujisan-${VERSION_CLEAN}-windows.zip" > "Fujisan-${VERSION_CLEAN}-windows.zip.sha256"
                echo "  SHA256: $(awk '{print $1}' "Fujisan-${VERSION_CLEAN}-windows.zip.sha256")"
                cd - > /dev/null
                
                # Clean up build directory after successful packaging
                rm -rf build-windows/
            else
                echo_error "Failed to create Windows zip package"
            fi
        else
            echo_error "Windows build directory not found"
        fi
    else
        echo_error "Windows build script not found"
        return 1
    fi
    
    echo_success "Windows build complete"
}

# Build Linux
build_linux() {
    local arch="${1:-amd64}"  # Default to amd64 for backward compatibility

    echo_step "Building Linux ($arch)"

    # Ensure we're in the project root
    cd "$PROJECT_ROOT"

    # Use the Docker-based Linux build
    if [[ -f "$SCRIPT_DIR/scripts/build-linux-docker.sh" ]]; then
        "$SCRIPT_DIR/scripts/build-linux-docker.sh" "$arch" --version "$VERSION"
    else
        echo_error "Linux build script not found"
        return 1
    fi

    echo_success "Linux build complete"
}

# Check dependencies for signing/notarization
check_macos_dependencies() {
    local missing_deps=()
    
    if [[ "$SIGN" == "true" ]] || [[ "$NOTARIZE" == "true" ]]; then
        if ! command -v codesign &> /dev/null; then
            missing_deps+=("codesign (Xcode Command Line Tools)")
        fi
        
        if ! command -v xcrun &> /dev/null; then
            missing_deps+=("xcrun (Xcode Command Line Tools)")
        fi
    fi
    
    if [[ "$NOTARIZE" == "true" ]] && ! command -v jq &> /dev/null; then
        missing_deps+=("jq (install with: brew install jq)")
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        echo_error "Missing required dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        exit 1
    fi
}

# Parse arguments
PLATFORM=""
CLEAN=false
SIGN=false
NOTARIZE=false
BUNDLE_FUJINET=false
DEVELOPER_ID=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=true
            shift
            ;;
        --sign)
            SIGN=true
            shift
            ;;
        --notarize)
            NOTARIZE=true
            SIGN=true  # Notarization requires signing
            shift
            ;;
        --bundle-fujinet)
            BUNDLE_FUJINET=true
            shift
            ;;
        --developer-id)
            DEVELOPER_ID="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        macos-arm64|macos-x86_64|macos|windows|linux|linux-x86_64|linux-amd64|linux-arm64|linux-aarch64|all)
            PLATFORM="$1"
            shift
            ;;
        *)
            echo_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Default to showing help if no platform specified
if [[ -z "$PLATFORM" ]]; then
    show_help
    exit 0
fi

# Create dist directories
mkdir -p "$MACOS_DIST_DIR" "$WINDOWS_DIST_DIR" "$LINUX_DIST_DIR"

# Create build directories for macOS
if [[ "$PLATFORM" == macos* ]] || [[ "$PLATFORM" == "all" ]]; then
    mkdir -p "$ARM64_BUILD_DIR" "$X86_64_BUILD_DIR"
fi

# Clean if requested
if [[ "$CLEAN" == "true" ]]; then
    clean_all
fi

# Check dependencies for macOS if needed
if [[ "$PLATFORM" == macos* ]] || ([[ "$PLATFORM" == "all" ]] && [[ "$OSTYPE" == "darwin"* ]]); then
    check_macos_dependencies
fi

# Execute builds based on platform
echo_step "Fujisan Build System"
echo_info "Version: $VERSION"
echo_info "Platform: $PLATFORM"
if [[ "$SIGN" == "true" ]]; then
    echo_info "Signing: Enabled"
fi
if [[ "$NOTARIZE" == "true" ]]; then
    echo_info "Notarization: Enabled"
fi
echo ""

case $PLATFORM in
    macos-arm64)
        build_macos_arm64
        ;;
    macos-x86_64)
        build_macos_x86_64
        ;;
    macos)
        build_macos_arm64
        build_macos_x86_64
        ;;
    windows)
        build_windows
        ;;
    linux|linux-x86_64|linux-amd64)
        build_linux "amd64"
        ;;
    linux-arm64|linux-aarch64)
        build_linux "arm64"
        ;;
    all)
        # Build for macOS if on macOS
        if [[ "$OSTYPE" == "darwin"* ]]; then
            build_macos_arm64
            build_macos_x86_64
        fi
        # Always build Windows and Linux (cross-platform builds)
        build_windows
        build_linux "amd64"
        build_linux "arm64"
        ;;
    *)
        echo_error "Unknown platform: $PLATFORM"
        exit 1
        ;;
esac

# Show summary
echo ""
echo_step "Build Summary"
echo_success "Build complete for: $PLATFORM"
echo ""
echo "Distribution files:"

# Show macOS files if they exist
if [[ -d "$MACOS_DIST_DIR" ]] && ls "$MACOS_DIST_DIR"/*.dmg >/dev/null 2>&1; then
    echo "  macOS (dist/macos/):"
    ls -lh "$MACOS_DIST_DIR"/*.dmg 2>/dev/null | awk '{print "    - "$9" ("$5")"}'
    if [[ "$SIGN" == "true" ]]; then
        echo "    Status: Signed with Developer ID"
    fi
    if [[ "$NOTARIZE" == "true" ]]; then
        echo "    Status: Notarized and stapled"
    fi
fi

# Show Windows files if they exist
if [[ -d "$WINDOWS_DIST_DIR" ]] && ls "$WINDOWS_DIST_DIR"/*.zip >/dev/null 2>&1; then
    echo "  Windows (dist/windows/):"
    ls -lh "$WINDOWS_DIST_DIR"/*.zip 2>/dev/null | awk '{print "    - "$9" ("$5")"}'
fi

# Show Linux files if they exist
if [[ -d "$LINUX_DIST_DIR" ]] && ls "$LINUX_DIST_DIR"/*.{deb,tar.gz} >/dev/null 2>&1; then
    echo "  Linux (dist/linux/):"
    ls -lh "$LINUX_DIST_DIR"/*.{deb,tar.gz} 2>/dev/null | awk '{print "    - "$9" ("$5")"}'
fi

echo ""

# Show next steps
if [[ "$PLATFORM" == macos* ]] || ([[ "$PLATFORM" == "all" ]] && [[ "$OSTYPE" == "darwin"* ]]); then
    if [[ "$SIGN" == "false" ]]; then
        echo "Note: DMGs use ad-hoc signing (for development only)"
        echo "For distribution, rebuild with: $0 $PLATFORM --sign --notarize"
    elif [[ "$NOTARIZE" == "false" ]]; then
        echo "Note: DMGs are signed but not notarized"
        echo "To notarize: $0 $PLATFORM --notarize"
    else
        echo "DMGs are signed, notarized, and ready for distribution!"
    fi
fi

echo ""
echo "To build for other platforms, run:"
echo "  $0 [platform] --clean"