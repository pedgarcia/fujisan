#!/bin/bash
#
# build-linux-docker.sh - Build Fujisan for Linux using Docker/Podman
#
# Based on the working GitHub Actions workflow
# Creates .deb package, .tar.gz portable, and optionally AppImage
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Output directory
LINUX_BUILD_DIR="${PROJECT_ROOT}/build-linux"
DIST_DIR="${PROJECT_ROOT}/dist/linux"

# Version
VERSION="${VERSION:-$(git describe --tags --always 2>/dev/null || echo "v1.0.0-dev")}"
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
        --version)
            VERSION="$2"
            VERSION_CLEAN=$(echo "$VERSION" | sed 's/^v//')
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
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
            echo "Output:"
            echo "  dist/fujisan_${VERSION_CLEAN}_amd64.deb     - Debian package"
            echo "  dist/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz - Portable tarball"
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

echo_info "Building Ubuntu 24.04 based container for x86_64..."
$CONTAINER_RUNTIME build \
    --platform linux/amd64 \
    -f docker/Dockerfile.ubuntu-24.04 \
    -t fujisan-linux-builder:ubuntu24 \
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

echo "Building Fujisan version $VERSION..."

# Copy source to writable location
cp -r /build/fujisan /tmp/fujisan-build
cd /tmp/fujisan-build

# Configure CMake
mkdir -p build-release
cd build-release

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DFUJISAN_VERSION="$VERSION" \
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

# Prepare installation directory
mkdir -p fujisan-linux/usr/bin
mkdir -p fujisan-linux/usr/share/applications
mkdir -p fujisan-linux/usr/share/pixmaps
mkdir -p fujisan-linux/usr/share/doc/fujisan

# Copy binary
cp Fujisan fujisan-linux/usr/bin/fujisan
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

# Copy images
mkdir -p fujisan-linux/usr/share/fujisan/images
cp ../images/*.png fujisan-linux/usr/share/fujisan/images/ 2>/dev/null || true

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
Architecture: amd64
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
Depends: libc6, libstdc++6, libgl1, libasound2, libqt5core5a, libqt5gui5, libqt5widgets5, libqt5multimedia5
EOF
    
    # Set proper permissions
    find fujisan-linux -type d -exec chmod 755 {} \;
    find fujisan-linux -type f -exec chmod 644 {} \;
    chmod 755 fujisan-linux/usr/bin/fujisan
    
    # Build .deb package
    dpkg-deb --build fujisan-linux "fujisan_${VERSION_CLEAN}_amd64.deb"
    
    echo "✓ .deb package created"
    ls -lh "fujisan_${VERSION_CLEAN}_amd64.deb"
    
    # Copy to output
    cp "fujisan_${VERSION_CLEAN}_amd64.deb" /output/
fi

# Create .tar.gz package
if [ "$BUILD_TARBALL" = "true" ]; then
    echo "Creating portable .tar.gz package..."
    
    # Create portable directory structure
    mkdir -p fujisan-portable/bin
    mkdir -p fujisan-portable/share/doc
    
    # Copy binary
    cp Fujisan fujisan-portable/bin/
    
    # Copy documentation
    [ -f "../README.md" ] && cp ../README.md fujisan-portable/share/doc/
    [ -f "../LICENSE" ] && cp ../LICENSE fujisan-portable/share/doc/
    
    # Copy images
    mkdir -p fujisan-portable/share/images
    cp ../images/*.png fujisan-portable/share/images/ 2>/dev/null || true
    
    # Create run script
    cat > fujisan-portable/fujisan.sh << 'EOF'
#!/bin/bash
# Fujisan Portable Launcher
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
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
- glibc 2.35+ (Ubuntu 22.04+, Debian 12+)
- Basic OpenGL support
- Qt5 libraries (will use system ones if available)

For system-wide installation, use the .deb package instead.

Visit: https://github.com/atari800/fujisan
EOF
    
    # Create tar.gz
    tar -czf "fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" fujisan-portable/
    
    echo "✓ .tar.gz package created"
    ls -lh "fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
    
    # Copy to output
    cp "fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" /output/
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

# Run build in container
echo_step "Building Fujisan in Container"

echo_info "Running build for version $VERSION on x86_64..."
$CONTAINER_RUNTIME run --rm \
    --platform linux/amd64 \
    -v "$PROJECT_ROOT:/build/fujisan:ro" \
    -v "$LINUX_BUILD_DIR:/output" \
    -e VERSION="$VERSION" \
    -e VERSION_CLEAN="$VERSION_CLEAN" \
    fujisan-linux-builder:ubuntu24 \
    bash /output/build-in-container.sh "$VERSION" "$VERSION_CLEAN" "$BUILD_DEB" "$BUILD_TARBALL" || {
    echo_error "Build failed"
    exit 1
}

echo_success "Linux build completed"

# Copy to dist directory
echo_step "Copying to Distribution Directory"

if [[ "$BUILD_DEB" == "true" ]] && [[ -f "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_amd64.deb" ]]; then
    cp "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_amd64.deb" "$DIST_DIR/"
    cp "$LINUX_BUILD_DIR/fujisan_${VERSION_CLEAN}_amd64.deb.sha256" "$DIST_DIR/" 2>/dev/null || true
    echo_success "Copied .deb package to dist/"
fi

if [[ "$BUILD_TARBALL" == "true" ]] && [[ -f "$LINUX_BUILD_DIR/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" ]]; then
    cp "$LINUX_BUILD_DIR/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" "$DIST_DIR/"
    cp "$LINUX_BUILD_DIR/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz.sha256" "$DIST_DIR/" 2>/dev/null || true
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
    $CONTAINER_RUNTIME rmi fujisan-linux-builder:ubuntu24 2>/dev/null || true
fi

# Summary
echo_step "Build Summary"
echo_success "Linux build completed successfully!"
echo ""
echo "Version: $VERSION"
echo "Output files in $DIST_DIR:"

if [[ -f "$DIST_DIR/fujisan_${VERSION_CLEAN}_amd64.deb" ]]; then
    SIZE=$(du -h "$DIST_DIR/fujisan_${VERSION_CLEAN}_amd64.deb" | cut -f1)
    echo "  • fujisan_${VERSION_CLEAN}_amd64.deb ($SIZE) - Debian/Ubuntu package"
fi

if [[ -f "$DIST_DIR/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" ]]; then
    SIZE=$(du -h "$DIST_DIR/fujisan-${VERSION_CLEAN}-linux-x64.tar.gz" | cut -f1)
    echo "  • fujisan-${VERSION_CLEAN}-linux-x64.tar.gz ($SIZE) - Portable package"
fi

echo ""
echo "Installation:"
echo "  Debian/Ubuntu: sudo dpkg -i fujisan_${VERSION_CLEAN}_amd64.deb"
echo "  Portable: tar xzf fujisan-${VERSION_CLEAN}-linux-x64.tar.gz && ./fujisan-portable/fujisan.sh"