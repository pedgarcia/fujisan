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

# Run entire build in container
podman run --rm \
    -v "$PROJECT_ROOT:/work" \
    --workdir="/work" \
    --platform linux/amd64 \
    maxrd2/arch-mingw \
    bash -c '
set -e

echo "=== Building in container ==="

# Clean and create build directory
rm -rf build-cross-windows
mkdir -p build-cross-windows
cd build-cross-windows

# Configure with CMake
echo "Configuring..."
x86_64-w64-mingw32-cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
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

# Copy system libraries
cp /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll build-windows/
cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll build-windows/
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

echo "✓ Windows package created in build-windows/"
'

echo ""
echo "Windows build complete!"
echo "Package is in: build-windows/"
ls -la build-windows/ 2>/dev/null | head -10