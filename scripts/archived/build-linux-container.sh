#!/bin/bash
# build-linux-container.sh - Build Fujisan for Linux using containers
#
# This script builds Fujisan for Linux using Docker/Podman containers
# to ensure consistent build environment across different host systems.
#

set -e

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Linux Container Build for Fujisan ==="
echo "Project Root: $PROJECT_ROOT"
echo ""

# Configuration
BUILD_DIR="build-linux-container"
CONTAINER_ENGINE=""
CONTAINER_IMAGE="ubuntu:22.04"
FUJINET_ENABLED=false
CLEAN_BUILD=false

# Detect container engine
detect_container_engine() {
    if command -v podman >/dev/null 2>&1; then
        CONTAINER_ENGINE="podman"
        echo "Using Podman as container engine"
    elif command -v docker >/dev/null 2>&1; then
        CONTAINER_ENGINE="docker"
        echo "Using Docker as container engine"
    else
        echo "Error: Neither Podman nor Docker found"
        echo "Please install Podman or Docker to continue"
        exit 1
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --fujinet)
            FUJINET_ENABLED=true
            shift
            ;;
        --image)
            CONTAINER_IMAGE="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --clean       Clean build directory before starting"
            echo "  --fujinet     Enable FujiNet/NetSIO support"
            echo "  --image IMG   Use specific container image (default: ubuntu:22.04)"
            echo "  --help        Show this help message"
            echo ""
            echo "Supported images:"
            echo "  ubuntu:22.04  - Ubuntu 22.04 LTS (default)"
            echo "  ubuntu:20.04  - Ubuntu 20.04 LTS"  
            echo "  fedora:38     - Fedora 38"
            echo "  archlinux     - Arch Linux (latest)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Detect container engine
detect_container_engine

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == "true" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$PROJECT_ROOT/$BUILD_DIR"
fi

# Create build directory
mkdir -p "$PROJECT_ROOT/$BUILD_DIR"

# Build script to run inside container
BUILD_SCRIPT=$(cat << 'EOF'
#!/bin/bash
set -e

echo "=== Setting up Linux build environment ==="

# Detect distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO="unknown"
fi

echo "Detected distribution: $DISTRO"

# Install dependencies based on distribution
case $DISTRO in
    ubuntu|debian)
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y \
            build-essential \
            cmake \
            git \
            autoconf \
            automake \
            libtool \
            pkg-config \
            qtbase5-dev \
            qtmultimedia5-dev \
            qttools5-dev-tools \
            libqt5network5 \
            qt5-qmake
        
        # Additional packages for FujiNet support
        if [ "$FUJINET_ENABLED" == "true" ]; then
            apt-get install -y libssl-dev
        fi
        ;;
    
    fedora)
        dnf install -y \
            gcc \
            gcc-c++ \
            cmake \
            git \
            autoconf \
            automake \
            libtool \
            pkgconfig \
            qt5-qtbase-devel \
            qt5-qtmultimedia-devel \
            qt5-qttools-devel \
            qt5-qtnetwork-devel
        
        if [ "$FUJINET_ENABLED" == "true" ]; then
            dnf install -y openssl-devel
        fi
        ;;
    
    arch|archlinux)
        pacman -Sy --noconfirm \
            base-devel \
            cmake \
            git \
            autoconf \
            automake \
            libtool \
            pkgconfig \
            qt5-base \
            qt5-multimedia \
            qt5-tools
        
        if [ "$FUJINET_ENABLED" == "true" ]; then
            pacman -S --noconfirm openssl
        fi
        ;;
    
    *)
        echo "Unsupported distribution: $DISTRO"
        echo "Please install the following packages manually:"
        echo "- build-essential/gcc/base-devel"
        echo "- cmake, git, autotools"
        echo "- Qt5 development packages"
        exit 1
        ;;
esac

echo ""
echo "=== Building Fujisan ==="

cd /work/$BUILD_DIR

# Configure CMake
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"
if [ "$FUJINET_ENABLED" == "true" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DENABLE_FUJINET=ON"
    echo "FujiNet/NetSIO support enabled"
fi

echo "Configuring with CMake..."
cmake .. $CMAKE_ARGS

echo "Building with make..."
make -j$(nproc)

echo ""
echo "=== Build Summary ==="
if [ -f "Fujisan" ]; then
    echo "✓ Fujisan executable built successfully"
    echo "Size: $(du -h Fujisan | cut -f1)"
    echo "Type: $(file Fujisan)"
    
    # Check libatari800
    if [ -f "atari800-src/src/libatari800.a" ]; then
        echo "✓ libatari800.a built successfully"  
        echo "Size: $(du -h atari800-src/src/libatari800.a | cut -f1)"
    fi
    
    # Check FujiNet support
    if [ "$FUJINET_ENABLED" == "true" ]; then
        if grep -q "#define NETSIO 1" atari800-src/src/config.h 2>/dev/null; then
            echo "✓ FujiNet/NetSIO support confirmed"
        else
            echo "⚠  FujiNet/NetSIO may not be enabled"
        fi
    fi
    
    echo ""
    echo "Build completed successfully!"
    echo "Executable: /work/$BUILD_DIR/Fujisan"
    
else
    echo "✗ Build failed - Fujisan executable not found"
    exit 1
fi
EOF
)

# Run build in container
echo "Starting Linux build in container..."
echo "Container: $CONTAINER_IMAGE"
echo "Build directory: $BUILD_DIR"
echo "FujiNet enabled: $FUJINET_ENABLED"
echo ""

$CONTAINER_ENGINE run --rm \
    -v "$PROJECT_ROOT:/work" \
    -w "/work" \
    -e "BUILD_DIR=$BUILD_DIR" \
    -e "FUJINET_ENABLED=$FUJINET_ENABLED" \
    "$CONTAINER_IMAGE" \
    bash -c "$BUILD_SCRIPT"

echo ""
echo "=== Linux Container Build Completed ==="
echo "✓ Build successful"
echo "✓ Output: $PROJECT_ROOT/$BUILD_DIR/Fujisan"

# Test executable
if [ -f "$PROJECT_ROOT/$BUILD_DIR/Fujisan" ]; then
    echo "✓ Executable created successfully"
    
    # Show ldd info if on Linux host
    if [[ "$(uname)" == "Linux" ]] && command -v ldd >/dev/null; then
        echo ""
        echo "Shared library dependencies:"
        ldd "$PROJECT_ROOT/$BUILD_DIR/Fujisan" 2>/dev/null || echo "ldd check failed (expected if cross-compiled)"
    fi
    
    echo ""
    echo "To run Fujisan:"
    echo "  cd $PROJECT_ROOT/$BUILD_DIR"
    echo "  ./Fujisan"
else
    echo "✗ Build failed - executable not found"
    exit 1
fi