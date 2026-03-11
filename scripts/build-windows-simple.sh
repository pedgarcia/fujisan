#!/bin/bash
# Simplified Windows build script that handles everything in one container run

set -e

echo "=== Simplified Windows Build ==="

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

# Clean previous builds
rm -rf build-cross-windows build-windows

echo "Building Windows executable and package..."

# Run entire build in container (pass BUILD_FASTBASIC_COMPILER and BUILD_FUJINET_PC from env if set)
podman run --rm \
    -v "$PROJECT_ROOT:/work" \
    --workdir="/work" \
    --platform linux/amd64 \
    --user root \
    -e VERSION="$VERSION" \
    -e BUILD_FASTBASIC_COMPILER="${BUILD_FASTBASIC_COMPILER:-false}" \
    -e BUILD_FUJINET_PC="${BUILD_FUJINET_PC:-true}" \
    maxrd2/arch-mingw \
    bash -c '
set -e

echo "=== Building in container ==="

# SDL2 MinGW libraries are already available in the container
echo "Using pre-installed SDL2 MinGW libraries..."

# Clean and create build directory
rm -rf build-cross-windows
mkdir -p build-cross-windows
cd build-cross-windows

# Configure with CMake (enable SDL2 for Windows joystick support)
echo "Configuring..."

# Get version from environment (passed from host)
if [[ -n "$VERSION" ]]; then
    echo "Using version from host environment: $VERSION"
    VERSION_CLEAN=$(echo "$VERSION" | sed "s/^v//")
else
    echo "ERROR: No VERSION environment variable provided from host"
    echo "Falling back to default version..."
    VERSION="v1.0.0-dev"
    VERSION_CLEAN="1.0.0-dev"
fi
echo "Building version: $VERSION_CLEAN"

x86_64-w64-mingw32-cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DPROJECT_VERSION="$VERSION_CLEAN" \
    ..

# Build
echo "Building..."
make -j$(nproc)

# Check for executable
if [ -f "Fujisan.exe" ]; then
    echo "✓ Fujisan.exe built successfully"
    ls -lh Fujisan.exe
else
    echo "ERROR: Fujisan.exe not found!"
    ls -la
    exit 1
fi

# Create release package
cd ..
rm -rf build-windows
mkdir -p build-windows

# Copy executable
cp build-cross-windows/Fujisan.exe build-windows/

# Copy Qt libraries
echo "Copying Qt libraries..."
cp /usr/x86_64-w64-mingw32/bin/Qt5Core.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/Qt5Gui.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/Qt5Widgets.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/Qt5Multimedia.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/Qt5Network.dll build-windows/

# Copy SDL2 libraries for joystick support
echo "Copying SDL2 libraries..."
if [ -f "/usr/x86_64-w64-mingw32/bin/SDL2.dll" ]; then
    cp /usr/x86_64-w64-mingw32/bin/SDL2.dll build-windows/
    echo "✓ SDL2.dll copied"
else
    echo "Warning: SDL2.dll not found - joystick support may not work"
fi

# Copy system libraries
cp /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libssp-0.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/zlib1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libpcre2-16-0.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libharfbuzz-0.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libpng16-16.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libfreetype-6.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libglib-2.0-0.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libintl-8.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libiconv-2.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libpcre-1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libbz2-1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libgraphite2.dll build-windows/

# Copy Qt plugins
mkdir -p build-windows/platforms
cp /usr/x86_64-w64-mingw32/lib/qt/plugins/platforms/qwindows.dll build-windows/platforms/

mkdir -p build-windows/audio  
cp /usr/x86_64-w64-mingw32/lib/qt/plugins/audio/qtaudio_windows.dll build-windows/audio/

mkdir -p build-windows/mediaservice
cp /usr/x86_64-w64-mingw32/lib/qt/plugins/mediaservice/qtmedia_audioengine.dll build-windows/mediaservice/

# Copy images
mkdir -p build-windows/images
cp -r images/*.png build-windows/images/ 2>/dev/null || true

# Bundle Fastbasic if requested (download release zip and extract full contents)
if [ "$BUILD_FASTBASIC_COMPILER" = "true" ]; then
    echo "Downloading Fastbasic v4.7 win32..."
    mkdir -p build-windows/fastbasic
    curl -sL -f -o /tmp/fb-win32.zip "https://github.com/dmsc/fastbasic/releases/download/v4.7/fastbasic-v4.7-win32.zip"
    unzip -q -o /tmp/fb-win32.zip -d build-windows/fastbasic
    rm -f /tmp/fb-win32.zip
    echo "✓ Fastbasic bundled in build-windows/fastbasic/"
fi

# Bundle FujiNet-PC if requested (copy from host fujinet/windows-x64 or download nightly)
if [ "$BUILD_FUJINET_PC" != "false" ]; then
    mkdir -p build-windows/fujinet-pc
    if [ -f "/work/fujinet/windows-x64/fujinet.exe" ]; then
        echo "Bundling FujiNet-PC from /work/fujinet/windows-x64..."
        cp -r /work/fujinet/windows-x64/* build-windows/fujinet-pc/
        echo "✓ FujiNet-PC bundled from host fujinet/windows-x64"
    else
        echo "Downloading FujiNet-PC Windows nightly..."
        GITHUB_API="https://api.github.com/repos/FujiNetWIFI/fujinet-firmware/releases"
        TAG=$(curl -s "$GITHUB_API" | grep -o "\"tag_name\"[[:space:]]*:[[:space:]]*\"nightly_fn_pc_[^\"]*\"" | head -n 1 | cut -d "\"" -f 4)
        if [ -n "$TAG" ]; then
            TAG_ENCODED=$(echo "$TAG" | sed "s/+/%2B/g")
            VERSION_PART="${TAG#nightly_fn_pc_}"
            ZIP_NAME="fujinet-pc-ATARI_${VERSION_PART}_windows-x64.zip"
            BASE_URL="https://github.com/FujiNetWIFI/fujinet-firmware/releases/download/$TAG_ENCODED"
            if curl -sL -f -o /tmp/fujinet-win.zip "$BASE_URL/$ZIP_NAME"; then
                unzip -q -o /tmp/fujinet-win.zip -d /tmp/fujinet-win-extract
                SUBDIR=$(find /tmp/fujinet-win-extract -maxdepth 1 -type d -name "fujinet-pc-*" | head -n 1)
                if [ -n "$SUBDIR" ]; then
                    cp -r "$SUBDIR"/* build-windows/fujinet-pc/
                else
                    cp -r /tmp/fujinet-win-extract/* build-windows/fujinet-pc/
                fi
                rm -rf /tmp/fujinet-win-extract /tmp/fujinet-win.zip
                echo "✓ FujiNet-PC bundled (nightly $TAG)"
            else
                echo "⚠ Warning: Could not download FujiNet-PC nightly; run scripts/download-fujinet-pc.sh and rebuild with fujinet/windows-x64 present"
            fi
        else
            echo "⚠ Warning: Could not find FujiNet-PC nightly tag; run scripts/download-fujinet-pc.sh and rebuild with fujinet/windows-x64 present"
        fi
    fi
fi

echo "✓ Windows package created in build-windows/"
'

echo ""
echo "Windows build complete!"
echo "Package is in: build-windows/"
ls -la build-windows/ 2>/dev/null | head -10