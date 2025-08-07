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
#   linux          - Build for Linux (using Docker/Podman)
#   all            - Build for all platforms
#
# Options:
#   --clean        - Clean before building
#   --sign         - Sign macOS builds (requires certificates)
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

# Version
VERSION="${VERSION:-$(git describe --tags --always 2>/dev/null || echo "v1.0.0-dev")}"
VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')

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
  linux          Build for Linux (Docker/Podman)
  all            Build for all platforms

Options:
  --clean        Clean before building
  --sign         Sign macOS builds
  --version X    Set version
  --help         Show this help

Examples:
  $0 macos                    # Build both Mac versions
  $0 windows --clean           # Clean build for Windows
  $0 all --version v1.2.0      # Build all platforms
  $0 all --sign --clean        # Build everything with signing

Output:
  All builds output to: dist/
  
  dist/macos/
    - Fujisan-{version}-arm64.dmg
    - Fujisan-{version}-x86_64.dmg
  dist/windows/
    - Fujisan-{version}-windows.zip
  dist/linux/
    - fujisan-{version}-linux-x64.tar.gz
    - fujisan_{version}_amd64.deb

EOF
}

# Clean function
clean_all() {
    echo_info "Cleaning all build directories..."
    rm -rf build/ build-*/ dist/*
    echo_success "Clean complete"
}

# Build macOS ARM64
build_macos_arm64() {
    echo_step "Building macOS ARM64"
    
    if [[ ! -d "/opt/homebrew/opt/qt@5" ]]; then
        echo_error "Qt5 for ARM64 not found. Install with: brew install qt@5"
        return 1
    fi
    
    # Use scripts/build-macos-separate-dmgs.sh if it exists
    if [[ -f "$SCRIPT_DIR/scripts/build-macos-separate-dmgs.sh" ]]; then
        "$SCRIPT_DIR/scripts/build-macos-separate-dmgs.sh" --skip-x86_64
    else
        echo_error "macOS build script not found"
        return 1
    fi
    
    echo_success "macOS ARM64 build complete"
}

# Build macOS x86_64
build_macos_x86_64() {
    echo_step "Building macOS x86_64"
    
    if [[ ! -d "/usr/local/opt/qt@5" ]]; then
        echo_error "Qt5 for x86_64 not found. Install with: arch -x86_64 /usr/local/bin/brew install qt@5"
        return 1
    fi
    
    # Use scripts/build-macos-separate-dmgs.sh if it exists
    if [[ -f "$SCRIPT_DIR/scripts/build-macos-separate-dmgs.sh" ]]; then
        "$SCRIPT_DIR/scripts/build-macos-separate-dmgs.sh" --skip-arm64
    else
        echo_error "macOS build script not found"
        return 1
    fi
    
    echo_success "macOS x86_64 build complete"
}

# Build Windows
build_windows() {
    echo_step "Building Windows"
    
    # Check for Docker/Podman
    if command -v podman &> /dev/null; then
        CONTAINER_RUNTIME="podman"
    elif command -v docker &> /dev/null; then
        CONTAINER_RUNTIME="docker"
    else
        echo_error "Neither podman nor docker found. Install one to build for Windows."
        return 1
    fi
    
    # Clean Windows build directory
    rm -rf build-cross-windows/
    
    # Use the consolidated Windows build script
    if [[ -f "$SCRIPT_DIR/create-windows-release-with-audio.sh" ]]; then
        "$SCRIPT_DIR/create-windows-release-with-audio.sh"
        
        # Package into zip for distribution
        if [[ -d "build-windows" ]]; then
            echo_info "Creating Windows distribution package..."
            mkdir -p "$WINDOWS_DIST_DIR"
            cd build-windows
            zip -r "$WINDOWS_DIST_DIR/Fujisan-${VERSION_CLEAN}-windows.zip" * -x "*.log"
            cd ..
            echo_success "Created: dist/windows/Fujisan-${VERSION_CLEAN}-windows.zip"
            
            # Clean up build directory after packaging
            rm -rf build-windows/
        fi
    else
        echo_error "Windows build script not found"
        return 1
    fi
    
    echo_success "Windows build complete"
}

# Build Linux
build_linux() {
    echo_step "Building Linux"
    
    # Use the Docker-based Linux build
    if [[ -f "$SCRIPT_DIR/scripts/build-linux-docker.sh" ]]; then
        "$SCRIPT_DIR/scripts/build-linux-docker.sh" --version "$VERSION"
    else
        echo_error "Linux build script not found"
        return 1
    fi
    
    echo_success "Linux build complete"
}

# Parse arguments
PLATFORM=""
CLEAN=false
SIGN=false

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
        --version)
            VERSION="$2"
            VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        macos-arm64|macos-x86_64|macos|windows|linux|all)
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

# Clean if requested
if [[ "$CLEAN" == "true" ]]; then
    clean_all
fi

# Execute builds based on platform
echo_step "Fujisan Build System"
echo_info "Version: $VERSION"
echo_info "Platform: $PLATFORM"
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
    linux)
        build_linux
        ;;
    all)
        # Detect current OS and build accordingly
        if [[ "$OSTYPE" == "darwin"* ]]; then
            build_macos_arm64
            build_macos_x86_64
        fi
        build_windows
        build_linux
        ;;
    *)
        echo_error "Unknown platform: $PLATFORM"
        exit 1
        ;;
esac

# Sign macOS builds if requested
if [[ "$SIGN" == "true" ]]; then
    # Sign if we built macOS (either directly or as part of 'all')
    if [[ "$PLATFORM" == macos* ]] || ([[ "$PLATFORM" == "all" ]] && [[ "$OSTYPE" == "darwin"* ]]); then
        if [[ -f "$SCRIPT_DIR/scripts/sign-macos-apps.sh" ]]; then
            echo_step "Signing macOS builds"
            "$SCRIPT_DIR/scripts/sign-macos-apps.sh"
        else
            echo_info "Signing script not found, skipping macOS signing"
        fi
    fi
fi

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
echo "To build for other platforms, run:"
echo "  $0 [platform] --clean"