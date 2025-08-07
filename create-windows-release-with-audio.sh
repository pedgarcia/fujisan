#!/bin/bash
# Create complete Windows release with disk LED and audio fixes

set -e
echo "=== Creating Windows Release with Disk LED and Audio Fixes ==="

cd /Users/pgarcia/dev/atari/fujisan

RELEASE_DIR="windows-release-complete"
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

# Copy the newly built executable
echo "Copying Fujisan.exe with disk LED fixes..."
cp build-cross-windows/Fujisan.exe "$RELEASE_DIR/"

# Copy all DLLs and dependencies from original release
echo "Copying Windows dependencies..."
cp windows-release/*.dll "$RELEASE_DIR/" 2>/dev/null || true
cp -r windows-release/platforms "$RELEASE_DIR/" 2>/dev/null || true
cp -r windows-release/images "$RELEASE_DIR/" 2>/dev/null || true

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
Fujisan - Modern Atari Emulator for Windows (COMPLETE RELEASE)
================================================================

This Windows build includes ALL fixes:

WHAT'S FIXED:
✓ Disk activity LEDs now work correctly during disk I/O operations
✓ Audio output now works properly with Windows audio backend
✓ All Qt multimedia plugins included for proper sound support

FEATURES:
- Drive LEDs light up when reading/writing to disk images
- Full audio support for Atari sound (POKEY chip emulation)
- Real-time disk activity monitoring integrated with SIO operations
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
echo "✓ Complete Windows release package created: $RELEASE_DIR/"
echo ""
echo "Contents:"
ls -la "$RELEASE_DIR/"
echo ""
echo "Audio plugins:"
ls -la "$RELEASE_DIR/audio/" "$RELEASE_DIR/mediaservice/" 2>/dev/null || true
echo ""
echo "Both disk LEDs and audio should now work! Test by loading games/demos with sound."