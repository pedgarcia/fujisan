#!/bin/bash
#
# build-linux-docker.sh - Build Fujisan for Linux using Docker/Podman
#
# Based on the working GitHub Actions workflow
# Creates .deb package, .tar.gz portable, and optionally AppImage
# Supports both x86_64 (amd64) and ARM64 (aarch64) architectures
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Architecture - first parameter or default to amd64
ARCH="${1:-amd64}"

# Validate architecture
if [[ "$ARCH" != "amd64" && "$ARCH" != "arm64" ]]; then
    echo "ERROR: Invalid architecture: $ARCH (must be amd64 or arm64)"
    exit 1
fi

# Shift to process remaining arguments
if [[ "$1" == "amd64" ]] || [[ "$1" == "arm64" ]]; then
    shift
fi

# Output directory
LINUX_BUILD_DIR="${PROJECT_ROOT}/build-linux-${ARCH}"
DIST_DIR="${PROJECT_ROOT}/dist/linux"

# Version
# Full version from git (e.g., "v1.0.5-9-g296092d")
VERSION="${VERSION:-$(git describe --tags --always 2>/dev/null || echo "v1.0.0-dev")}"
# Remove 'v' prefix (e.g., "1.0.5-9-g296092d")
# Note: CMakeLists.txt will extract numeric version for CMake's project(VERSION)
VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')

# Container runtime detection
if command -v podman &> /dev/null; then
    CONTAINER_RUNTIME="podman"
elif command -v docker &> /dev/null; then
    CONTAINER_RUNTIME="docker"
else
    echo "Error: Neither podman nor docker found. Please install one of them."
    echo "  macOS: brew install podman"
    echo "  Linux: sudo apt install podman (or docker)"
    exit 1
fi

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

# Parse command line arguments
CLEAN_BUILD=false
BUILD_DEB=true
BUILD_TARBALL=true
BUILD_APPIMAGE=false
KEEP_CONTAINER=false
BUILD_FUJINET_PC=false
BUILD_FASTBASIC_COMPILER=false
FUJINET_SRC_DIR=""
FASTBASIC_SRC_DIR=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --no-deb)
            BUILD_DEB=false
            shift
            ;;
        --no-tarball)
            BUILD_TARBALL=false
            shift
            ;;
        --appimage)
            BUILD_APPIMAGE=true
            shift
            ;;
        --keep-container)
            KEEP_CONTAINER=true
            shift
            ;;
        --build-fujinet-pc)
            BUILD_FUJINET_PC=true
            shift
            ;;
        --build-fastbasic-compiler)
            BUILD_FASTBASIC_COMPILER=true
            shift
            ;;
        --fastbasic-src)
            FASTBASIC_SRC_DIR="$2"
            shift 2
            ;;
        --fujinet-src)
            FUJINET_SRC_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')
            shift 2
            ;;
        --help)
            echo "Usage: $0 [arch] [options]"
            echo ""
            echo "Architecture:"
            echo "  amd64            Build for x86_64 (default)"
            echo "  arm64            Build for ARM64/aarch64"
            echo ""
            echo "Options:"
            echo "  --clean          Clean build directories before starting"
            echo "  --no-deb         Skip .deb package creation"
            echo "  --no-tarball     Skip .tar.gz creation"
            echo "  --appimage       Also create AppImage (experimental)"
            echo "  --keep-container Keep container image after build"
            echo "  --version        Set version (default: from git)"
            echo "  --help           Show this help message"
            echo ""
            echo "Output (amd64):"
            echo "  dist/linux/fujisan_${VERSION_CLEAN}_amd64.deb"
            echo "  dist/linux/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
            echo ""
            echo "Output (arm64):"
            echo "  dist/linux/fujisan_${VERSION_CLEAN}_arm64.deb"
            echo "  dist/linux/fujisan-${VERSION_CLEAN}-linux-arm64.tar.gz"
            echo ""
            echo "Using: $CONTAINER_RUNTIME"
            exit 0
            ;;
        *)
            echo_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Main execution
echo_step "Linux Build for Fujisan"
echo_info "Using container runtime: $CONTAINER_RUNTIME"
echo_info "Architecture: $ARCH"
echo_info "Project root: $PROJECT_ROOT"
echo_info "Version: $VERSION"
echo ""

# Clean if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    echo_info "Cleaning build directory..."
    rm -rf "$LINUX_BUILD_DIR"
fi

# Create output directories
mkdir -p "$LINUX_BUILD_DIR"
mkdir -p "$DIST_DIR"

# Build container image
echo_step "Building Container Image"
cd "$PROJECT_ROOT"

echo_info "Building Ubuntu 22.04 based container for $ARCH..."
$CONTAINER_RUNTIME build \
    --platform linux/$ARCH \
    -f docker/Dockerfile.ubuntu-22.04 \
    -t fujisan-linux-builder:ubuntu22-$ARCH \
    . || {
    echo_error "Failed to build container image"
    exit 1
}
echo_success "Container image built"

# Create build script that will run inside container
cat > "$LINUX_BUILD_DIR/build-in-container.sh" << 'EOSCRIPT'
#!/bin/bash
set -e

VERSION="$1"
VERSION_CLEAN="$2"
BUILD_DEB="$3"
BUILD_TARBALL="$4"
ARCH="$5"

echo "Building Fujisan version $VERSION..."

# Copy source to writable location
cp -r /build/fujisan /tmp/fujisan-build
cd /tmp/fujisan-build

# Configure CMake
mkdir -p build-release
cd build-release

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DPROJECT_VERSION="$VERSION_CLEAN" \
      ..

# Build
echo "Building with $(nproc) cores..."
make -j$(nproc)

# Verify libatari800 integration
if [ -f "atari800-src/src/libatari800.a" ]; then
    echo "✓ libatari800.a found and integrated"
    ls -lh atari800-src/src/libatari800.a
else
    echo "❌ libatari800.a not found"
    exit 1
fi

# Test binary
echo "Testing Fujisan binary..."
file Fujisan
ldd Fujisan | head -20

# Verify architecture matches expected
echo ""
echo "=== Architecture Verification ==="
FUJISAN_ARCH=$(file Fujisan | grep -o 'ELF [0-9]*-bit' | awk '{print $2}')
FUJISAN_MACHINE=$(file Fujisan | grep -o -E '(x86-64|aarch64|ARM aarch64)')

if [ "$ARCH" = "amd64" ]; then
    EXPECTED_BITS="64"
    EXPECTED_MACHINE="x86-64"
elif [ "$ARCH" = "arm64" ]; then
    EXPECTED_BITS="64"
    EXPECTED_MACHINE="aarch64"
fi

echo "Expected architecture: $ARCH ($EXPECTED_MACHINE, ${EXPECTED_BITS}-bit)"
echo "Fujisan binary: $FUJISAN_MACHINE, ${FUJISAN_ARCH}-bit"

# Check libatari800.a architecture
if [ -f "atari800-src/src/libatari800.a" ]; then
    echo ""
    echo "libatari800.a architecture:"
    file atari800-src/src/libatari800.a

    # For .a archive files, we need to extract and check an object file
    # Create temp directory for extraction
    TEMP_CHECK_DIR=$(mktemp -d)
    cd "$TEMP_CHECK_DIR"

    # Extract first object file from archive
    ar x ../atari800-src/src/libatari800.a 2>/dev/null | head -1

    # Get the first .o file
    FIRST_OBJ=$(ls *.o 2>/dev/null | head -1)

    if [ -n "$FIRST_OBJ" ]; then
        echo "Checking object file: $FIRST_OBJ"
        file "$FIRST_OBJ"

        LIBATARI_MACHINE=$(file "$FIRST_OBJ" | grep -o -E '(x86-64|aarch64|ARM aarch64)')
        echo "libatari800.a machine type: $LIBATARI_MACHINE"

        # Verify libatari800 matches expected architecture
        if [ "$ARCH" = "amd64" ] && [[ "$LIBATARI_MACHINE" != *"x86-64"* ]]; then
            cd - > /dev/null
            rm -rf "$TEMP_CHECK_DIR"
            echo "❌ ERROR: libatari800.a is not x86-64! This will cause performance issues."
            exit 1
        elif [ "$ARCH" = "arm64" ] && [[ "$LIBATARI_MACHINE" != *"aarch64"* ]] && [[ "$LIBATARI_MACHINE" != *"ARM aarch64"* ]]; then
            cd - > /dev/null
            rm -rf "$TEMP_CHECK_DIR"
            echo "❌ ERROR: libatari800.a is not ARM64/aarch64! This will cause performance issues."
            exit 1
        else
            echo "✓ libatari800.a architecture verified: $LIBATARI_MACHINE"
        fi
    else
        echo "⚠ Warning: Could not extract object file from libatari800.a for verification"
    fi

    # Clean up and return to build directory
    cd - > /dev/null
    rm -rf "$TEMP_CHECK_DIR"
fi

# Verify Fujisan binary matches expected architecture
if [ "$ARCH" = "amd64" ] && [[ "$FUJISAN_MACHINE" != *"x86-64"* ]]; then
    echo "❌ ERROR: Fujisan binary is not x86-64!"
    exit 1
elif [ "$ARCH" = "arm64" ] && [[ "$FUJISAN_MACHINE" != *"aarch64"* ]] && [[ "$FUJISAN_MACHINE" != *"ARM aarch64"* ]]; then
    echo "❌ ERROR: Fujisan binary is not ARM64/aarch64!"
    exit 1
else
    echo "✓ Fujisan binary architecture verified: $FUJISAN_MACHINE"
fi

echo "=== Architecture Verification Complete ==="
echo ""

# Prepare installation directory
mkdir -p fujisan-linux/usr/bin
mkdir -p fujisan-linux/usr/share/applications
mkdir -p fujisan-linux/usr/share/pixmaps
mkdir -p fujisan-linux/usr/share/doc/fujisan

# Create library directory for bundled libraries
mkdir -p fujisan-linux/usr/lib/fujisan
mkdir -p fujisan-linux/usr/lib/fujisan/plugins/platforms
mkdir -p fujisan-linux/usr/lib/fujisan/plugins/audio
mkdir -p fujisan-linux/usr/lib/fujisan/plugins/mediaservice
mkdir -p fujisan-linux/usr/lib/fujisan/plugins/imageformats
mkdir -p fujisan-linux/usr/lib/fujisan/plugins/xcbglintegrations

# Bundle FujiNet-PC for .deb package
mkdir -p fujisan-linux/usr/lib/fujisan/fujinet-pc

# FUJINET_SOURCE: the platform folder in the fujisan repo — always has data, config, SD, scripts.
# The binary is either pre-built (from here) or freshly compiled (BUILD_FUJINET_PC=true).
if [ "$ARCH" = "amd64" ]; then
    FUJINET_SOURCE="/build/fujisan/fujinet/ubuntu-22.04"
elif [ "$ARCH" = "arm64" ]; then
    FUJINET_SOURCE="/build/fujisan/fujinet/ubuntu-22.04-arm64"
fi

# Optionally build the FujiNet-PC binary from source
FUJINET_BINARY=""
if [ "$BUILD_FUJINET_PC" = "true" ]; then
    if [ ! -d "/build/fujinet-firmware" ]; then
        echo "❌ ERROR: /build/fujinet-firmware not mounted but --build-fujinet-pc was requested"
        exit 1
    fi
    echo "Building FujiNet-PC from source inside container..."
    mkdir -p /tmp/fujinet-build
    cd /tmp/fujinet-build
    cmake /build/fujinet-firmware \
        -DFUJINET_TARGET=ATARI \
        -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    # dist/ mirrors build.sh: WebUI + distfiles. The dist CMake target runs build_webui.py
    # (needs python3-jinja2, python3-yaml) and copies distfiles/. If it fails, assemble the same layout.
    echo "Running FujiNet dist target (WebUI + distfiles → dist/)..."
    if cmake --build . --target dist; then
        echo "✓ cmake dist target completed"
    else
        echo "⚠ cmake dist failed; running build_webui and assembling dist/ manually..."
        if cmake --build . --target build_webui; then
            mkdir -p /tmp/fujinet-build/dist
            cp -r /build/fujinet-firmware/distfiles/. /tmp/fujinet-build/dist/
            cp -f /tmp/fujinet-build/fujinet /tmp/fujinet-build/dist/fujinet
            if [ -d /tmp/fujinet-build/data ]; then
                rm -rf /tmp/fujinet-build/dist/data
                cp -r /tmp/fujinet-build/data /tmp/fujinet-build/dist/data
                echo "✓ Manual dist/ layout created (data + distfiles + fujinet)"
            else
                echo "⚠ build_webui did not produce /tmp/fujinet-build/data — check Python deps (jinja2, yaml)"
            fi
        else
            echo "⚠ build_webui failed — bundle may lack data/; ensure python3-jinja2 and python3-yaml are installed"
        fi
    fi
    FUJINET_BINARY="/tmp/fujinet-build/fujinet"
    if [ -f "/tmp/fujinet-build/dist/fujinet" ]; then
        FUJINET_BINARY="/tmp/fujinet-build/dist/fujinet"
    fi
    echo "✓ FujiNet-PC built: $(file $FUJINET_BINARY)"
    cd /tmp/fujisan-build/build-release
fi

# Copy all FujiNet files from source folder (data, config, SD, scripts, etc.)
if [ -d "$FUJINET_SOURCE" ]; then
    echo "Bundling FujiNet-PC from $FUJINET_SOURCE..."

    # Binary: use freshly built one if available, otherwise use pre-built
    BINARY_SRC="${FUJINET_BINARY:-$FUJINET_SOURCE/fujinet}"
    cp "$BINARY_SRC" fujisan-linux/usr/lib/fujisan/fujinet-pc/fujinet
    chmod 755 fujisan-linux/usr/lib/fujisan/fujinet-pc/fujinet

    # Data directory (firmware assets, certs, fonts, etc.)
    if [ -d "$FUJINET_SOURCE/data" ]; then
        cp -r "$FUJINET_SOURCE/data" fujisan-linux/usr/lib/fujisan/fujinet-pc/
    fi

    # Main config file
    if [ -f "$FUJINET_SOURCE/fnconfig.ini" ]; then
        cp "$FUJINET_SOURCE/fnconfig.ini" fujisan-linux/usr/lib/fujisan/fujinet-pc/
    fi

    # SD card directory (copy contents, preserving any files already there)
    mkdir -p fujisan-linux/usr/lib/fujisan/fujinet-pc/SD
    if [ -d "$FUJINET_SOURCE/SD" ]; then
        cp -r "$FUJINET_SOURCE/SD/." fujisan-linux/usr/lib/fujisan/fujinet-pc/SD/
    fi

    # Optional extras
    [ -f "$FUJINET_SOURCE/run-fujinet.sh" ] && cp "$FUJINET_SOURCE/run-fujinet.sh" fujisan-linux/usr/lib/fujisan/fujinet-pc/
    [ -f "$FUJINET_SOURCE/run-fujinet" ]    && cp "$FUJINET_SOURCE/run-fujinet"    fujisan-linux/usr/lib/fujisan/fujinet-pc/
    [ -f "$FUJINET_SOURCE/VERSION" ]        && cp "$FUJINET_SOURCE/VERSION"        fujisan-linux/usr/lib/fujisan/fujinet-pc/

    echo "✓ FujiNet-PC bundled for .deb"
elif [ -n "$FUJINET_BINARY" ]; then
    echo "Bundling FujiNet-PC from source build..."
    cp "$FUJINET_BINARY" fujisan-linux/usr/lib/fujisan/fujinet-pc/fujinet
    chmod 755 fujisan-linux/usr/lib/fujisan/fujinet-pc/fujinet
    mkdir -p fujisan-linux/usr/lib/fujisan/fujinet-pc/SD

    FUJINET_DIST="/tmp/fujinet-build/dist"
    # Prefer CMake dist/ output (matches build.sh); fall back to legacy repo layout
    if [ -d "$FUJINET_DIST/data" ]; then
        echo "Copying data from $FUJINET_DIST/data..."
        cp -r "$FUJINET_DIST/data" fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ Data folder copied"
    elif [ -d "/tmp/fujinet-build/data" ]; then
        echo "Copying data from CMake build tree (BUILD_DATA_DIR)..."
        cp -r /tmp/fujinet-build/data fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ Data folder copied"
    elif [ -d "/build/fujinet-firmware/data" ]; then
        echo "Copying data from fujinet-firmware repo root (legacy layout)..."
        cp -r /build/fujinet-firmware/data fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ Data folder copied"
    else
        echo "⚠ Warning: No data folder found under dist/, build tree, or repo root"
    fi

    if [ -f "$FUJINET_DIST/fnconfig.ini" ]; then
        cp "$FUJINET_DIST/fnconfig.ini" fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ fnconfig.ini copied from dist"
    elif [ -f "/build/fujinet-firmware/distfiles/fnconfig.ini" ]; then
        cp /build/fujinet-firmware/distfiles/fnconfig.ini fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ fnconfig.ini copied from distfiles"
    elif [ -f "/build/fujinet-firmware/data/webui/device_specific/BUILD_ATARI/fnconfig.ini" ]; then
        cp /build/fujinet-firmware/data/webui/device_specific/BUILD_ATARI/fnconfig.ini \
           fujisan-linux/usr/lib/fujisan/fujinet-pc/
        echo "✓ fnconfig.ini copied from BUILD_ATARI"
    fi

    if [ -d "$FUJINET_DIST/SD" ]; then
        cp -r "$FUJINET_DIST/SD/." fujisan-linux/usr/lib/fujisan/fujinet-pc/SD/
    fi
    [ -f "$FUJINET_DIST/run-fujinet.sh" ] && cp "$FUJINET_DIST/run-fujinet.sh" fujisan-linux/usr/lib/fujisan/fujinet-pc/
    [ -f "$FUJINET_DIST/run-fujinet" ]    && cp "$FUJINET_DIST/run-fujinet"    fujisan-linux/usr/lib/fujisan/fujinet-pc/
    [ -f "$FUJINET_DIST/VERSION" ]        && cp "$FUJINET_DIST/VERSION"        fujisan-linux/usr/lib/fujisan/fujinet-pc/

    echo "✓ FujiNet-PC bundled for .deb (from source)"
else
    echo "⚠ Warning: No FujiNet-PC found to bundle"
fi

# Bundle Fastbasic if requested
mkdir -p fujisan-linux/usr/lib/fujisan/fastbasic
if [ "$BUILD_FASTBASIC_COMPILER" = "true" ]; then
    if [ "$ARCH" = "amd64" ]; then
        echo "Downloading Fastbasic v4.7 linux64..."
        curl -sL -f -o /tmp/fb-linux64.zip "https://github.com/dmsc/fastbasic/releases/download/v4.7/fastbasic-v4.7-linux64.zip"
        unzip -q -o /tmp/fb-linux64.zip -d fujisan-linux/usr/lib/fujisan/fastbasic
        rm -f /tmp/fb-linux64.zip
        chmod 755 fujisan-linux/usr/lib/fujisan/fastbasic/fastbasic fujisan-linux/usr/lib/fujisan/fastbasic/fb 2>/dev/null || true
        echo "✓ Fastbasic (linux64) bundled"
    elif [ "$ARCH" = "arm64" ] && [ -d "/build/fastbasic" ]; then
        echo "Building Fastbasic from source (v4.7)..."
        FB_DEST="/tmp/fujisan-build/build-release/fujisan-linux/usr/lib/fujisan/fastbasic"
        # The source is mounted read-only and may contain x86_64 build artifacts from
        # the host. Copy to a writable directory so we can do a clean native ARM64 build.
        FB_BUILD_DIR="/tmp/fastbasic-src"
        rm -rf "$FB_BUILD_DIR"
        cp -r /build/fastbasic "$FB_BUILD_DIR"
        cd "$FB_BUILD_DIR"
        git fetch --tags 2>/dev/null || true
        git checkout v4.7 2>/dev/null || git checkout 4.7 2>/dev/null || true
        make clean 2>/dev/null || true
        make -j$(nproc)
        cp -p build/bin/fastbasic build/bin/fb build/bin/ca65 build/bin/ld65 build/bin/ar65 build/bin/mkatr build/bin/lsatr "$FB_DEST/" 2>/dev/null || true
        cp -p build/compiler/*.lib build/compiler/*.cfg build/compiler/*.tgt "$FB_DEST/" 2>/dev/null || true
        cp -r build/compiler/asminc build/compiler/syntax "$FB_DEST/" 2>/dev/null || true
        chmod 755 "$FB_DEST"/fastbasic "$FB_DEST"/fb 2>/dev/null || true
        cd /tmp/fujisan-build/build-release
        echo "✓ Fastbasic (arm64 from source) bundled"
    elif [ "$ARCH" = "arm64" ]; then
        echo "⚠ Warning: BUILD_FASTBASIC_COMPILER requested for arm64 but /build/fastbasic not mounted (use --fastbasic-src)"
    fi
fi

# Copy binary to lib directory (real binary)
cp Fujisan fujisan-linux/usr/lib/fujisan/Fujisan
chmod 755 fujisan-linux/usr/lib/fujisan/Fujisan

# Create qt.conf to prevent Qt from searching system plugin paths
# This is critical for preventing version conflicts on KDE systems
cat > fujisan-linux/usr/lib/fujisan/qt.conf << 'QTCONF_EOF'
[Paths]
Plugins = plugins
QTCONF_EOF

# Copy Qt libraries
echo "Copying Qt libraries..."
for lib in libQt5Core libQt5Gui libQt5Widgets libQt5Multimedia libQt5Network libQt5DBus libQt5XcbQpa; do
    find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-linux/usr/lib/fujisan/ \; 2>/dev/null || true
done

# Copy SDL2 libraries if available
echo "Copying SDL2 libraries..."
for lib in libSDL2-2.0; do
    find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-linux/usr/lib/fujisan/ \; 2>/dev/null || true
done

# Copy Qt plugins
echo "Copying Qt plugins..."
# Platform plugin (required for GUI)
find /usr/lib -path "*/qt5/plugins/platforms/libqxcb.so" -exec cp {} fujisan-linux/usr/lib/fujisan/plugins/platforms/ \; 2>/dev/null || true
# Audio plugins
find /usr/lib -path "*/qt5/plugins/audio/*.so" -exec cp {} fujisan-linux/usr/lib/fujisan/plugins/audio/ \; 2>/dev/null || true
# Media service plugins
find /usr/lib -path "*/qt5/plugins/mediaservice/*.so" -exec cp {} fujisan-linux/usr/lib/fujisan/plugins/mediaservice/ \; 2>/dev/null || true
# Image format plugins
find /usr/lib -path "*/qt5/plugins/imageformats/*.so" -exec cp {} fujisan-linux/usr/lib/fujisan/plugins/imageformats/ \; 2>/dev/null || true
# XCB GL integrations
find /usr/lib -path "*/qt5/plugins/xcbglintegrations/*.so" -exec cp {} fujisan-linux/usr/lib/fujisan/plugins/xcbglintegrations/ \; 2>/dev/null || true

# Copy additional system libraries that Qt depends on
echo "Copying system dependencies..."
# Core system libraries
for lib in libxcb-xkb libxkbcommon-x11 libxkbcommon libicui18n libicuuc libicudata libmd4c libdouble-conversion libpcre2-16; do
    find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-linux/usr/lib/fujisan/ \; 2>/dev/null || true
done
# X11 libraries
for lib in libXss libXrandr libXrender libXfixes libXcursor libXinerama libXext libXi libxcb-xinerama libxcb-xinput libxcb-randr libxcb-render libxcb-shape libxcb-shm libxcb-icccm libxcb-image libxcb-keysyms libxcb-render-util; do
    find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-linux/usr/lib/fujisan/ \; 2>/dev/null || true
done

# Create wrapper script in /usr/bin
cat > fujisan-linux/usr/bin/fujisan << 'WRAPPER_EOF'
#!/bin/bash
# Fujisan wrapper script
# Note: qt.conf (next to binary) prevents Qt from searching system plugin paths
export LD_LIBRARY_PATH="/usr/lib/fujisan"
export FUJISAN_IMAGES_PATH="/usr/share/fujisan/images"
# Disable KDE platform theme integration to prevent loading system Qt plugins
export QT_QPA_PLATFORMTHEME=""
# Use bundled Fusion style for consistent cross-platform appearance
export QT_STYLE_OVERRIDE="Fusion"
exec /usr/lib/fujisan/Fujisan "$@"
WRAPPER_EOF
chmod 755 fujisan-linux/usr/bin/fujisan

# Create desktop file
cat > fujisan-linux/usr/share/applications/fujisan.desktop << EOF
[Desktop Entry]
Name=Fujisan
Comment=Modern Atari 8-bit Emulator
Exec=fujisan
Icon=fujisan
Terminal=false
Type=Application
Categories=Game;Emulator;
Keywords=atari;emulator;8bit;retro;gaming;
StartupNotify=true
EOF

# Copy icon
if [ -f "../images/FujisanLogoIcon.png" ]; then
    cp ../images/FujisanLogoIcon.png fujisan-linux/usr/share/pixmaps/fujisan.png
elif [ -f "../images/fujisanlogo.png" ]; then
    cp ../images/fujisanlogo.png fujisan-linux/usr/share/pixmaps/fujisan.png
fi

# Copy documentation
[ -f "../README.md" ] && cp ../README.md fujisan-linux/usr/share/doc/fujisan/
[ -f "../LICENSE" ] && cp ../LICENSE fujisan-linux/usr/share/doc/fujisan/

# Copy ALL images (including device status indicators)
mkdir -p fujisan-linux/usr/share/fujisan/images
echo "Copying image resources..."
if [ -d "../images" ]; then
    # Copy all PNG files
    for img in ../images/*.png; do
        if [ -f "$img" ]; then
            cp "$img" fujisan-linux/usr/share/fujisan/images/
            echo "  - Copied $(basename $img)"
        fi
    done
fi

# Create .deb package
if [ "$BUILD_DEB" = "true" ]; then
    echo "Creating .deb package..."
    
    # Create DEBIAN control directory
    mkdir -p fujisan-linux/DEBIAN
    
    # Calculate installed size (in KB)
    INSTALLED_SIZE=$(du -sk fujisan-linux/usr | cut -f1)
    
    cat > fujisan-linux/DEBIAN/control << EOF
Package: fujisan
Version: $VERSION_CLEAN
Section: games
Priority: optional
Architecture: $ARCH
Essential: no
Installed-Size: $INSTALLED_SIZE
Maintainer: 8bitrelics.com <noreply@8bitrelics.com>
Homepage: https://github.com/atari800/fujisan
Description: Modern Atari 8-bit Emulator
 Fujisan is a modern, Qt5-based Atari 8-bit computer emulator.
 It provides accurate emulation of Atari 400/800/XL/XE systems
 with a user-friendly interface and modern features.
 .
 This package includes all necessary Qt5 libraries and libatari800
 for a complete, self-contained installation.
Depends: libc6, libstdc++6, libgl1, libasound2
EOF
    
    # Set proper permissions
    find fujisan-linux -type d -exec chmod 755 {} \;
    find fujisan-linux -type f -exec chmod 644 {} \;
    # Restore executable bit for all known executables
    chmod 755 fujisan-linux/usr/bin/fujisan
    chmod 755 fujisan-linux/usr/lib/fujisan/Fujisan
    # Fastbasic executables
    for fb_bin in fastbasic fb ca65 ld65 ar65 mkatr lsatr; do
        chmod 755 "fujisan-linux/usr/lib/fujisan/fastbasic/$fb_bin" 2>/dev/null || true
    done
    # FujiNet executables
    chmod 755 fujisan-linux/usr/lib/fujisan/fujinet-pc/fujinet 2>/dev/null || true
    chmod 755 fujisan-linux/usr/lib/fujisan/fujinet-pc/run-fujinet.sh 2>/dev/null || true
    chmod 755 fujisan-linux/usr/lib/fujisan/fujinet-pc/run-fujinet 2>/dev/null || true
    # Shared libraries need the execute bit to be loadable
    find fujisan-linux -name "*.so*" -type f -exec chmod 755 {} \; 2>/dev/null || true
    
    # Build .deb package
    dpkg-deb --build fujisan-linux "fujisan_${VERSION_CLEAN}_${ARCH}.deb"

    echo "✓ .deb package created"
    ls -lh "fujisan_${VERSION_CLEAN}_${ARCH}.deb"

    # Copy to output
    cp "fujisan_${VERSION_CLEAN}_${ARCH}.deb" /output/
fi

# Create .tar.gz package
if [ "$BUILD_TARBALL" = "true" ]; then
    echo "Creating portable .tar.gz package..."
    
    # Create portable directory structure
    mkdir -p fujisan-portable/bin
    mkdir -p fujisan-portable/share/doc
    
    # Copy binary and libraries
    cp Fujisan fujisan-portable/bin/

    # Create qt.conf to prevent Qt from searching system plugin paths
    # This is critical for preventing version conflicts on KDE systems
    cat > fujisan-portable/bin/qt.conf << 'QTCONF_EOF'
[Paths]
Plugins = ../plugins
QTCONF_EOF

    # Bundle FujiNet-PC for portable package
    # FUJINET_SOURCE and FUJINET_BINARY are already set from the .deb section above
    mkdir -p fujisan-portable/bin/fujinet-pc

    if [ -d "$FUJINET_SOURCE" ]; then
        echo "Bundling FujiNet-PC for portable package..."

        BINARY_SRC="${FUJINET_BINARY:-$FUJINET_SOURCE/fujinet}"
        cp "$BINARY_SRC" fujisan-portable/bin/fujinet-pc/fujinet
        chmod +x fujisan-portable/bin/fujinet-pc/fujinet

        if [ -d "$FUJINET_SOURCE/data" ]; then
            cp -r "$FUJINET_SOURCE/data" fujisan-portable/bin/fujinet-pc/
        fi

        if [ -f "$FUJINET_SOURCE/fnconfig.ini" ]; then
            cp "$FUJINET_SOURCE/fnconfig.ini" fujisan-portable/bin/fujinet-pc/
        fi

        mkdir -p fujisan-portable/bin/fujinet-pc/SD
        if [ -d "$FUJINET_SOURCE/SD" ]; then
            cp -r "$FUJINET_SOURCE/SD/." fujisan-portable/bin/fujinet-pc/SD/
        fi

        [ -f "$FUJINET_SOURCE/run-fujinet.sh" ] && cp "$FUJINET_SOURCE/run-fujinet.sh" fujisan-portable/bin/fujinet-pc/
        [ -f "$FUJINET_SOURCE/run-fujinet" ]    && cp "$FUJINET_SOURCE/run-fujinet"    fujisan-portable/bin/fujinet-pc/
        [ -f "$FUJINET_SOURCE/VERSION" ]        && cp "$FUJINET_SOURCE/VERSION"        fujisan-portable/bin/fujinet-pc/

        echo "✓ FujiNet-PC bundled in portable package"
    elif [ -n "$FUJINET_BINARY" ]; then
        echo "Bundling FujiNet-PC for portable package (from source)..."
        cp "$FUJINET_BINARY" fujisan-portable/bin/fujinet-pc/fujinet
        chmod +x fujisan-portable/bin/fujinet-pc/fujinet
        mkdir -p fujisan-portable/bin/fujinet-pc/SD

        FUJINET_DIST="/tmp/fujinet-build/dist"
        if [ -d "$FUJINET_DIST/data" ]; then
            echo "Copying data from $FUJINET_DIST/data..."
            cp -r "$FUJINET_DIST/data" fujisan-portable/bin/fujinet-pc/
            echo "✓ Data folder copied"
        elif [ -d "/tmp/fujinet-build/data" ]; then
            echo "Copying data from CMake build tree (BUILD_DATA_DIR)..."
            cp -r /tmp/fujinet-build/data fujisan-portable/bin/fujinet-pc/
            echo "✓ Data folder copied"
        elif [ -d "/build/fujinet-firmware/data" ]; then
            echo "Copying data from fujinet-firmware repo root (legacy layout)..."
            cp -r /build/fujinet-firmware/data fujisan-portable/bin/fujinet-pc/
            echo "✓ Data folder copied"
        else
            echo "⚠ Warning: No data folder found under dist/, build tree, or repo root"
        fi

        if [ -f "$FUJINET_DIST/fnconfig.ini" ]; then
            cp "$FUJINET_DIST/fnconfig.ini" fujisan-portable/bin/fujinet-pc/
            echo "✓ fnconfig.ini copied from dist"
        elif [ -f "/build/fujinet-firmware/distfiles/fnconfig.ini" ]; then
            cp /build/fujinet-firmware/distfiles/fnconfig.ini fujisan-portable/bin/fujinet-pc/
            echo "✓ fnconfig.ini copied from distfiles"
        elif [ -f "/build/fujinet-firmware/data/webui/device_specific/BUILD_ATARI/fnconfig.ini" ]; then
            cp /build/fujinet-firmware/data/webui/device_specific/BUILD_ATARI/fnconfig.ini \
               fujisan-portable/bin/fujinet-pc/
            echo "✓ fnconfig.ini copied from BUILD_ATARI"
        fi

        if [ -d "$FUJINET_DIST/SD" ]; then
            cp -r "$FUJINET_DIST/SD/." fujisan-portable/bin/fujinet-pc/SD/
        fi
        [ -f "$FUJINET_DIST/run-fujinet.sh" ] && cp "$FUJINET_DIST/run-fujinet.sh" fujisan-portable/bin/fujinet-pc/
        [ -f "$FUJINET_DIST/run-fujinet" ]    && cp "$FUJINET_DIST/run-fujinet"    fujisan-portable/bin/fujinet-pc/
        [ -f "$FUJINET_DIST/VERSION" ]        && cp "$FUJINET_DIST/VERSION"        fujisan-portable/bin/fujinet-pc/

        echo "✓ FujiNet-PC bundled in portable package (from source)"
    fi

    # Copy Fastbasic to portable if bundled
    if [ -d "fujisan-linux/usr/lib/fujisan/fastbasic" ] && [ -f "fujisan-linux/usr/lib/fujisan/fastbasic/fastbasic" ]; then
        echo "Copying Fastbasic to portable package..."
        cp -r fujisan-linux/usr/lib/fujisan/fastbasic fujisan-portable/bin/
        echo "✓ Fastbasic bundled in portable package"
    fi

    # Fix executable permissions for portable package.
    # The DEB step may have chmod 644'd everything; re-apply 755 to all executables.
    chmod 755 fujisan-portable/bin/Fujisan 2>/dev/null || true
    for fb_bin in fastbasic fb ca65 ld65 ar65 mkatr lsatr; do
        chmod 755 "fujisan-portable/bin/fastbasic/$fb_bin" 2>/dev/null || true
    done
    chmod 755 fujisan-portable/bin/fujinet-pc/fujinet 2>/dev/null || true
    chmod 755 fujisan-portable/bin/fujinet-pc/run-fujinet.sh 2>/dev/null || true
    chmod 755 fujisan-portable/bin/fujinet-pc/run-fujinet 2>/dev/null || true
    find fujisan-portable -name "*.so*" -type f -exec chmod 755 {} \; 2>/dev/null || true

    # Copy Qt libraries
    mkdir -p fujisan-portable/lib
    echo "Copying Qt libraries for portable package..."
    for lib in libQt5Core libQt5Gui libQt5Widgets libQt5Multimedia libQt5Network libQt5DBus libQt5XcbQpa; do
        find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-portable/lib/ \; 2>/dev/null || true
    done
    
    # Copy system dependencies
    echo "Copying system dependencies for portable package..."
    # Core system libraries
    for lib in libxcb-xkb libxkbcommon-x11 libxkbcommon libicui18n libicuuc libicudata libmd4c libdouble-conversion libpcre2-16; do
        find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-portable/lib/ \; 2>/dev/null || true
    done
    # X11 libraries
    for lib in libXss libXrandr libXrender libXfixes libXcursor libXinerama libXext libXi libxcb-xinerama libxcb-xinput libxcb-randr libxcb-render libxcb-shape libxcb-shm libxcb-icccm libxcb-image libxcb-keysyms libxcb-render-util; do
        find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-portable/lib/ \; 2>/dev/null || true
    done

    # Copy SDL2 libraries for joystick support
    echo "Copying SDL2 libraries for portable package..."
    for lib in libSDL2-2.0; do
        find /usr/lib -name "${lib}.so*" -exec cp {} fujisan-portable/lib/ \; 2>/dev/null || true
    done

    # Copy Qt plugins
    mkdir -p fujisan-portable/plugins/platforms
    mkdir -p fujisan-portable/plugins/audio
    mkdir -p fujisan-portable/plugins/mediaservice
    mkdir -p fujisan-portable/plugins/imageformats
    mkdir -p fujisan-portable/plugins/xcbglintegrations
    
    find /usr/lib -path "*/qt5/plugins/platforms/libqxcb.so" -exec cp {} fujisan-portable/plugins/platforms/ \; 2>/dev/null || true
    find /usr/lib -path "*/qt5/plugins/audio/*.so" -exec cp {} fujisan-portable/plugins/audio/ \; 2>/dev/null || true
    find /usr/lib -path "*/qt5/plugins/mediaservice/*.so" -exec cp {} fujisan-portable/plugins/mediaservice/ \; 2>/dev/null || true
    find /usr/lib -path "*/qt5/plugins/imageformats/*.so" -exec cp {} fujisan-portable/plugins/imageformats/ \; 2>/dev/null || true
    find /usr/lib -path "*/qt5/plugins/xcbglintegrations/*.so" -exec cp {} fujisan-portable/plugins/xcbglintegrations/ \; 2>/dev/null || true
    
    # Copy documentation
    [ -f "../README.md" ] && cp ../README.md fujisan-portable/share/doc/
    [ -f "../LICENSE" ] && cp ../LICENSE fujisan-portable/share/doc/
    
    # Copy ALL images (including device status indicators)
    mkdir -p fujisan-portable/images
    echo "Copying image resources for portable package..."
    if [ -d "../images" ]; then
        for img in ../images/*.png; do
            if [ -f "$img" ]; then
                cp "$img" fujisan-portable/images/
            fi
        done
    fi
    
    # Create run script
    cat > fujisan-portable/fujisan.sh << 'EOF'
#!/bin/bash
# Fujisan Portable Launcher
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Note: qt.conf (in bin/) prevents Qt from searching system plugin paths
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib"
export FUJISAN_IMAGES_PATH="$SCRIPT_DIR/images"
# Disable KDE platform theme integration to prevent loading system Qt plugins
export QT_QPA_PLATFORMTHEME=""
# Use bundled Fusion style for consistent cross-platform appearance
export QT_STYLE_OVERRIDE="Fusion"

exec "$SCRIPT_DIR/bin/Fujisan" "$@"
EOF
    chmod +x fujisan-portable/fujisan.sh
    
    # Create README for portable version
    cat > fujisan-portable/README-PORTABLE.txt << EOF
Fujisan Portable - Modern Atari 8-bit Emulator
===============================================

Version: $VERSION_CLEAN

To run Fujisan:
1. Extract this archive to any directory
2. Run: ./fujisan.sh

Requirements:
- Linux x86_64
- glibc 2.31+ (Ubuntu 20.04+, Debian 11+, RHEL/Rocky 8+)
- Basic OpenGL support
- X11 display server

This package includes all required Qt5 libraries and plugins.

For system-wide installation, use the .deb package instead.

Visit: https://github.com/atari800/fujisan
EOF
    
    # Create tar.gz with appropriate naming
    if [[ "$ARCH" == "amd64" ]]; then
        TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
    else
        TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-${ARCH}.tar.gz"
    fi

    tar -czf "$TARBALL_NAME" fujisan-portable/

    echo "✓ .tar.gz package created"
    ls -lh "$TARBALL_NAME"

    # Copy to output
    cp "$TARBALL_NAME" /output/
fi

# Generate checksums
echo "Generating checksums..."
cd /output
for file in fujisan_*.deb fujisan-*.tar.gz; do
    if [ -f "$file" ]; then
        sha256sum "$file" > "$file.sha256"
        echo "SHA256 for $file:"
        cat "$file.sha256"
    fi
done

echo "Build completed successfully!"
EOSCRIPT

chmod +x "$LINUX_BUILD_DIR/build-in-container.sh"

# Resolve fujinet-firmware path for --build-fujinet-pc
FUJINET_VOLUME_ARG=""
if [[ "$BUILD_FUJINET_PC" == "true" ]]; then
    if [[ -z "$FUJINET_SRC_DIR" ]]; then
        FUJINET_SRC_DIR="$(dirname "$PROJECT_ROOT")/fujinet-firmware"
    fi
    if [[ ! -d "$FUJINET_SRC_DIR" ]]; then
        echo_error "fujinet-firmware not found at: $FUJINET_SRC_DIR"
        echo_error "Clone it or set FUJINET_SOURCE_DIR to its path"
        exit 1
    fi
    FUJINET_VOLUME_ARG="-v $FUJINET_SRC_DIR:/build/fujinet-firmware:ro"
    echo_info "FujiNet-PC source: $FUJINET_SRC_DIR"
fi

# Resolve fastbasic path for --build-fastbasic-compiler (arm64: build from source)
FASTBASIC_VOLUME_ARG=""
if [[ "$BUILD_FASTBASIC_COMPILER" == "true" ]] && [[ "$ARCH" == "arm64" ]]; then
    if [[ -z "$FASTBASIC_SRC_DIR" ]]; then
        FASTBASIC_SRC_DIR="$(dirname "$PROJECT_ROOT")/fastbasic"
    fi
    if [[ ! -d "$FASTBASIC_SRC_DIR" ]]; then
        echo_error "fastbasic source not found at: $FASTBASIC_SRC_DIR (required for arm64)"
        echo_error "Clone it or set --fastbasic-src to its path"
        exit 1
    fi
    FASTBASIC_VOLUME_ARG="-v $FASTBASIC_SRC_DIR:/build/fastbasic:ro"
    echo_info "Fastbasic source (arm64): $FASTBASIC_SRC_DIR"
fi

# Run build in container
echo_step "Building Fujisan in Container"

echo_info "Running build for version $VERSION on $ARCH..."
$CONTAINER_RUNTIME run --rm \
    --platform linux/$ARCH \
    -v "$PROJECT_ROOT:/build/fujisan:ro" \
    -v "$LINUX_BUILD_DIR:/output" \
    ${FUJINET_VOLUME_ARG:+$FUJINET_VOLUME_ARG} \
    ${FASTBASIC_VOLUME_ARG:+$FASTBASIC_VOLUME_ARG} \
    -e VERSION="$VERSION" \
    -e VERSION_CLEAN="$VERSION_CLEAN" \
    -e ARCH="$ARCH" \
    -e BUILD_FUJINET_PC="$BUILD_FUJINET_PC" \
    -e BUILD_FASTBASIC_COMPILER="$BUILD_FASTBASIC_COMPILER" \
    fujisan-linux-builder:ubuntu22-$ARCH \
    bash /output/build-in-container.sh "$VERSION" "$VERSION_CLEAN" "$BUILD_DEB" "$BUILD_TARBALL" "$ARCH" || {
    echo_error "Build failed"
    exit 1
}

echo_success "Linux build completed"

# Copy to dist directory
echo_step "Copying to Distribution Directory"

# Determine tarball name based on architecture
if [[ "$ARCH" == "amd64" ]]; then
    TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
else
    TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-${ARCH}.tar.gz"
fi

if [[ "$BUILD_DEB" == "true" ]] && [[ -f "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_${ARCH}.deb" ]]; then
    cp "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_${ARCH}.deb" "$DIST_DIR/"
    cp "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_${ARCH}.deb.sha256" "$DIST_DIR/" 2>/dev/null || true
    echo_success "Copied .deb package to dist/"
fi

if [[ "$BUILD_TARBALL" == "true" ]] && [[ -f "$LINUX_BUILD_DIR/$TARBALL_NAME" ]]; then
    cp "$LINUX_BUILD_DIR/$TARBALL_NAME" "$DIST_DIR/"
    cp "$LINUX_BUILD_DIR/$TARBALL_NAME.sha256" "$DIST_DIR/" 2>/dev/null || true
    echo_success "Copied .tar.gz package to dist/"
fi

# Create AppImage if requested
if [[ "$BUILD_APPIMAGE" == "true" ]]; then
    echo_step "Creating AppImage (Experimental)"
    echo_info "AppImage creation requires additional setup..."
    echo_info "This is experimental and may not work in all environments"
    # AppImage creation would go here if needed
fi

# Clean up container if not keeping
if [[ "$KEEP_CONTAINER" == "false" ]]; then
    echo_info "Removing container image..."
    $CONTAINER_RUNTIME rmi fujisan-linux-builder:ubuntu22-$ARCH 2>/dev/null || true
fi

# Summary
echo_step "Build Summary"
echo_success "Linux build completed successfully!"
echo ""
echo "Architecture: $ARCH"
echo "Version: $VERSION"
echo "Output files in $DIST_DIR:"

# Determine tarball name for summary
if [[ "$ARCH" == "amd64" ]]; then
    TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
else
    TARBALL_NAME="fujisan-${VERSION_CLEAN}-linux-${ARCH}.tar.gz"
fi

if [[ -f "$DIST_DIR/fujisan_${VERSION_CLEAN}_${ARCH}.deb" ]]; then
    SIZE=$(du -h "$DIST_DIR/fujisan_${VERSION_CLEAN}_${ARCH}.deb" | cut -f1)
    echo "  • fujisan_${VERSION_CLEAN}_${ARCH}.deb ($SIZE) - Debian/Ubuntu package"
fi

if [[ -f "$DIST_DIR/$TARBALL_NAME" ]]; then
    SIZE=$(du -h "$DIST_DIR/$TARBALL_NAME" | cut -f1)
    echo "  • $TARBALL_NAME ($SIZE) - Portable package"
fi

echo ""
echo "Installation:"
echo "  Debian/Ubuntu: sudo dpkg -i fujisan_${VERSION_CLEAN}_${ARCH}.deb"
echo "  Portable: tar xzf $TARBALL_NAME && ./fujisan-portable/fujisan.sh"