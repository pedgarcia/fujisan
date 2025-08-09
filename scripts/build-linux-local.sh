#!/bin/bash
#
# build-linux-local.sh - Build Fujisan for Linux using Docker/Podman
#
# This script builds Linux binaries and creates an AppImage distribution
# using containerization for consistency across different host systems.
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Output directory
LINUX_BUILD_DIR="${PROJECT_ROOT}/build-linux"
DIST_DIR="${PROJECT_ROOT}/dist"

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
BUILD_APPIMAGE=true
KEEP_CONTAINER=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --no-appimage)
            BUILD_APPIMAGE=false
            shift
            ;;
        --keep-container)
            KEEP_CONTAINER=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean          Clean build directories before starting"
            echo "  --no-appimage    Skip AppImage creation"
            echo "  --keep-container Keep container image after build"
            echo "  --help           Show this help message"
            echo ""
            echo "Output:"
            echo "  build-linux/fujisan         - Linux executable"
            echo "  build-linux/Fujisan.AppImage - Portable AppImage (if enabled)"
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
echo ""

# Clean if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    echo_info "Cleaning build directory..."
    rm -rf "$LINUX_BUILD_DIR"
fi

# Create output directory
mkdir -p "$LINUX_BUILD_DIR"
mkdir -p "$DIST_DIR"

# Build container image
echo_step "Building Container Image"
cd "$PROJECT_ROOT"

echo_info "Building Ubuntu 22.04 based container..."
$CONTAINER_RUNTIME build \
    -f docker/Dockerfile.ubuntu-build \
    -t fujisan-linux-builder \
    . || {
    echo_error "Failed to build container image"
    exit 1
}
echo_success "Container image built"

# Run build in container
echo_step "Building Fujisan in Container"

echo_info "Running build..."
$CONTAINER_RUNTIME run --rm \
    -v "$LINUX_BUILD_DIR:/output" \
    fujisan-linux-builder \
    bash -c "
        set -e
        echo 'Building Fujisan for Linux...'
        
        # Create build directory
        cd /build/fujisan
        mkdir -p build
        cd build
        
        # Configure (disable SDL2 for container build)
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE \
              ..
        
        # Build
        make -j\$(nproc)
        
        # Copy binary
        cp Fujisan /output/fujisan
        chmod +x /output/fujisan
        
        # Copy libraries needed
        echo 'Collecting dependencies...'
        mkdir -p /output/lib
        ldd Fujisan | grep '=>' | awk '{print \$3}' | grep -E '(Qt5|fujisan)' | while read lib; do
            if [ -f \"\$lib\" ]; then
                cp \"\$lib\" /output/lib/
            fi
        done
        
        echo 'Build complete!'
        ls -la /output/
    " || {
    echo_error "Build failed"
    exit 1
}

echo_success "Linux binary built"

# Create AppImage if requested
if [[ "$BUILD_APPIMAGE" == "true" ]]; then
    echo_step "Creating AppImage"
    
    echo_info "Preparing AppImage structure..."
    
    # Create a temporary container for AppImage creation
    $CONTAINER_RUNTIME run --rm \
        -v "$LINUX_BUILD_DIR:/output" \
        -v "$PROJECT_ROOT:/source:ro" \
        --privileged \
        fujisan-linux-builder \
        bash -c "
            set -e
            cd /tmp
            
            # Create AppDir
            mkdir -p AppDir/usr/bin
            mkdir -p AppDir/usr/lib
            mkdir -p AppDir/usr/share/applications
            mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps
            
            # Copy binary
            cp /output/fujisan AppDir/usr/bin/
            
            # Create desktop file
            cat > AppDir/usr/share/applications/fujisan.desktop << 'EOFD'
[Desktop Entry]
Type=Application
Name=Fujisan
Comment=Atari 8-bit Computer Emulator
Exec=fujisan
Icon=fujisan
Categories=Game;Emulator;
Terminal=false
StartupNotify=true
EOFD
            
            # Copy icon
            if [ -f /source/images/fujisanlogo.png ]; then
                cp /source/images/fujisanlogo.png AppDir/usr/share/icons/hicolor/256x256/apps/fujisan.png
            fi
            
            # Create AppRun
            cat > AppDir/AppRun << 'EOFR'
#!/bin/bash
SELF=\$(readlink -f \"\$0\")
HERE=\${SELF%/*}
export PATH=\"\${HERE}/usr/bin:\${PATH}\"
export LD_LIBRARY_PATH=\"\${HERE}/usr/lib:\${LD_LIBRARY_PATH}\"
export QT_PLUGIN_PATH=\"\${HERE}/usr/lib/qt5/plugins\"
exec \"\${HERE}/usr/bin/fujisan\" \"\$@\"
EOFR
            chmod +x AppDir/AppRun
            
            # Use linuxdeploy to bundle Qt dependencies
            echo 'Downloading linuxdeploy...'
            wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
            wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
            chmod +x *.AppImage
            
            # Extract (can't run AppImage with FUSE in container)
            ./linuxdeploy-x86_64.AppImage --appimage-extract
            ./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract
            
            # Run linuxdeploy
            export PATH=\$(pwd)/squashfs-root/usr/bin:\$PATH
            ./squashfs-root/AppRun --appdir AppDir \
                --executable AppDir/usr/bin/fujisan \
                --desktop-file AppDir/usr/share/applications/fujisan.desktop \
                --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/fujisan.png \
                --plugin qt \
                --output appimage || true
            
            # If AppImage creation failed, create manually
            if [ ! -f Fujisan*.AppImage ]; then
                echo 'Creating AppImage manually...'
                wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
                chmod +x appimagetool-x86_64.AppImage
                ./appimagetool-x86_64.AppImage --appimage-extract
                ARCH=x86_64 ./squashfs-root/AppRun AppDir /output/Fujisan-x86_64.AppImage || {
                    echo 'Manual AppImage creation also failed, creating tarball instead'
                    tar czf /output/Fujisan-x86_64.tar.gz AppDir/
                }
            else
                cp Fujisan*.AppImage /output/
            fi
            
            echo 'AppImage packaging complete!'
        " || {
        echo_error "AppImage creation failed"
        echo_info "You can still use the binary at: $LINUX_BUILD_DIR/fujisan"
    }
    
    if [[ -f "$LINUX_BUILD_DIR/Fujisan-x86_64.AppImage" ]]; then
        echo_success "AppImage created"
        
        # Copy to dist directory
        cp "$LINUX_BUILD_DIR/Fujisan-x86_64.AppImage" "$DIST_DIR/"
        echo_success "AppImage copied to dist/"
    elif [[ -f "$LINUX_BUILD_DIR/Fujisan-x86_64.tar.gz" ]]; then
        echo_info "Created tarball instead of AppImage (FUSE not available in container)"
        cp "$LINUX_BUILD_DIR/Fujisan-x86_64.tar.gz" "$DIST_DIR/"
    fi
fi

# Create simple run script
echo_step "Creating Run Script"
cat > "$LINUX_BUILD_DIR/run-fujisan.sh" << 'EOF'
#!/bin/bash
# Run script for Fujisan on Linux
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
exec "${SCRIPT_DIR}/fujisan" "$@"
EOF
chmod +x "$LINUX_BUILD_DIR/run-fujisan.sh"
echo_success "Run script created"

# Clean up container if not keeping
if [[ "$KEEP_CONTAINER" == "false" ]]; then
    echo_info "Removing container image..."
    $CONTAINER_RUNTIME rmi fujisan-linux-builder 2>/dev/null || true
fi

# Summary
echo_step "Build Complete"
echo_success "Linux build successful!"
echo ""
echo "Output files:"
echo "  Binary: $LINUX_BUILD_DIR/fujisan"
echo "  Run script: $LINUX_BUILD_DIR/run-fujisan.sh"

if [[ -f "$LINUX_BUILD_DIR/Fujisan-x86_64.AppImage" ]]; then
    echo "  AppImage: $LINUX_BUILD_DIR/Fujisan-x86_64.AppImage"
    echo ""
    echo "The AppImage is portable and includes all dependencies."
    echo "It can run on most Linux distributions without installation."
elif [[ -f "$LINUX_BUILD_DIR/Fujisan-x86_64.tar.gz" ]]; then
    echo "  Tarball: $LINUX_BUILD_DIR/Fujisan-x86_64.tar.gz"
    echo ""
    echo "Extract and run with:"
    echo "  tar xzf Fujisan-x86_64.tar.gz"
    echo "  ./AppDir/AppRun"
fi

echo ""
echo "To run directly:"
echo "  $LINUX_BUILD_DIR/run-fujisan.sh"