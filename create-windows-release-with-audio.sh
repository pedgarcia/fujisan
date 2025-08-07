#!/bin/bash
# Create complete Windows release package

set -e
echo "=== Creating Windows Release Package ==="

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# First build the Windows executable
echo "Building Windows executable..."
if [[ -f "scripts/build-windows-simple.sh" ]]; then
    ./scripts/build-windows-simple.sh
elif [[ -f "scripts/build-windows-cross.sh" ]]; then
    ./scripts/build-windows-cross.sh
else
    echo "Error: Windows build script not found"
    exit 1
fi

RELEASE_DIR="build-windows"
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

# Copy the newly built executable
echo "Copying Fujisan.exe..."
if [[ -f "build-cross-windows/Fujisan.exe" ]]; then
    cp build-cross-windows/Fujisan.exe "$RELEASE_DIR/"
elif [[ -f "build-cross-windows/fujisan.exe" ]]; then
    cp build-cross-windows/fujisan.exe "$RELEASE_DIR/Fujisan.exe"
else
    echo "Error: Fujisan.exe not found in build-cross-windows/"
    echo "Contents of build-cross-windows:"
    ls -la build-cross-windows/ 2>/dev/null || echo "Directory not found"
    exit 1
fi

# Extract Qt and system DLLs from container
echo "Extracting Qt libraries and dependencies..."
podman run --rm -v $(pwd):/mnt maxrd2/arch-mingw bash -c "
    # Qt5 Core libraries
    cp /usr/x86_64-w64-mingw32/bin/Qt5Core.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/Qt5Gui.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/Qt5Widgets.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/Qt5Multimedia.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/Qt5Network.dll /mnt/$RELEASE_DIR/
    
    # System libraries
    cp /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/zlib1.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libpcre2-16-0.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libharfbuzz-0.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libpng16-16.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libfreetype-6.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libglib-2.0-0.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libintl-8.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libiconv-2.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libpcre-1.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libbz2-1.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libgraphite2.dll /mnt/$RELEASE_DIR/
    cp /usr/x86_64-w64-mingw32/bin/libssp-0.dll /mnt/$RELEASE_DIR/
    
    # Qt platform plugin (required)
    mkdir -p /mnt/$RELEASE_DIR/platforms
    cp /usr/x86_64-w64-mingw32/lib/qt/plugins/platforms/qwindows.dll /mnt/$RELEASE_DIR/platforms/
"

# Create audio and mediaservice plugin directories
echo "Setting up Qt audio plugins..."
mkdir -p "$RELEASE_DIR/audio"
mkdir -p "$RELEASE_DIR/mediaservice"

# Copy audio plugins from container
echo "Copying audio plugins from MinGW container..."
podman run --rm -v $(pwd):/mnt maxrd2/arch-mingw bash -c "
    cp /usr/x86_64-w64-mingw32/lib/qt/plugins/audio/qtaudio_windows.dll /mnt/$RELEASE_DIR/audio/
    cp /usr/x86_64-w64-mingw32/lib/qt/plugins/mediaservice/qtmedia_audioengine.dll /mnt/$RELEASE_DIR/mediaservice/
"

# Create README
cat > "$RELEASE_DIR/README.txt" << 'EOF'
Fujisan - Modern Atari Emulator for Windows
=============================================

FEATURES:
- Full Atari 400/800/XL/XE emulation
- Drive activity LED indicators
- Audio support with POKEY chip emulation
- Drag and drop disk/cartridge loading
- TCP server for remote control
- Debugger with breakpoints and memory inspection
- Windows audio backend properly configured

HOW TO TEST:
1. Run Fujisan.exe
2. Load a disk image or cartridge
3. Boot the system
4. You should hear audio and see disk LEDs during activity!

Plugin Structure:
- audio/qtaudio_windows.dll - Windows audio backend
- mediaservice/qtmedia_audioengine.dll - Audio engine support
- platforms/qwindows.dll - Windows platform integration

Build date: $(date)
EOF

echo ""
echo "✓ Windows release package created: $RELEASE_DIR/"
echo ""
echo "Contents:"
ls -la "$RELEASE_DIR/"
echo ""
echo "Plugin directories:"
ls -la "$RELEASE_DIR/audio/" "$RELEASE_DIR/mediaservice/" "$RELEASE_DIR/platforms/" 2>/dev/null || true
echo ""
echo "Package ready for distribution!"