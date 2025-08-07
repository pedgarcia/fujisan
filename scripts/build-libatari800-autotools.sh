#!/bin/bash
# build-libatari800-autotools.sh - Build libatari800 using full autotools in MinGW environment

set -e

echo "=== Building libatari800 with full autotools support ==="
echo "PWD: $(pwd)"

ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
fi

cd "$ATARI800_SRC_PATH"

# Ensure MinGW-w64 tools are in PATH for Windows builds
export PATH="/usr/sbin:/usr/bin:$PATH"
echo "Updated PATH for MinGW-w64: $PATH"

# Ensure Fujisan patches are applied for required API functions
if [ -d "fujisan-patches" ] && [ -f "fujisan-patches/apply-patches.sh" ]; then
    echo "Applying Fujisan patches for API functions..."
    cd fujisan-patches
    chmod +x apply-patches.sh
    ./apply-patches.sh || echo "Patches may already be applied"
    cd ..
else
    echo "Warning: Fujisan patches not found - some API functions may be missing"
fi

echo "=== Running autotools build process ==="

# Clean any previous build artifacts
echo "Cleaning previous build artifacts..."
make distclean 2>/dev/null || true
rm -f config.cache config.log config.status 2>/dev/null || true

# Generate configure script if needed
if [ ! -f configure ]; then
    echo "Running autogen.sh to generate configure script..."
    if [ -f autogen.sh ]; then
        chmod +x autogen.sh
        ./autogen.sh
    else
        echo "Error: autogen.sh not found and configure script missing"
        exit 1
    fi
fi

# Configure for libatari800 target with MinGW cross-compilation
echo "Configuring for libatari800 target..."

# Set cross-compilation environment variables
export CC="x86_64-w64-mingw32-gcc"
export CXX="x86_64-w64-mingw32-g++"
export AR="x86_64-w64-mingw32-ar"
export RANLIB="x86_64-w64-mingw32-ranlib"
export STRIP="x86_64-w64-mingw32-strip"
export PKG_CONFIG_PATH="/usr/x86_64-w64-mingw32/lib/pkgconfig"

# Configure with appropriate flags for Windows cross-compilation
./configure \
    --host=x86_64-w64-mingw32 \
    --target=libatari800 \
    --disable-sdltest \
    --disable-riodevice \
    --disable-netsio \
    --without-x \
    --disable-xep80 \
    --enable-libatari800 \
    --disable-crashmenu \
    --disable-monitorbreak \
    --disable-newcycleexact \
    --disable-volonlysound \
    --disable-interpolatesound \
    --disable-consoleoutput \
    --disable-seriosound \
    --disable-clipsound \
    --enable-nonlinearmixing \
    CFLAGS="-O2 -DTARGET_LIBATARI800 -DHAVE_CONFIG_H" \
    CPPFLAGS="-I/usr/x86_64-w64-mingw32/include" \
    LDFLAGS="-L/usr/x86_64-w64-mingw32/lib"

echo "=== Configuration completed ==="

# Apply Windows-specific fixes for ULONG type conflicts
echo "Applying Windows-specific fixes..."
sed -i 's/#include <windows\.h>/#include <windows.h>\n\/* Force atari800 ULONG definition to override Windows *\/\n#ifdef ULONG\n#undef ULONG\n#endif\n#define ULONG unsigned int/g' src/atari.h
sed -i '/^ULONG ANTIC_lookup_gtia9\[16\];$/d' src/antic.c  
sed -i '/^ULONG ANTIC_lookup_gtia11\[16\];$/d' src/antic.c

# Build libatari800
echo "Building libatari800..."
make clean || true
make -j$(nproc) libatari800.a || {
    echo "Full build failed, trying src/libatari800.a target specifically..."
    make -C src libatari800.a
} 

# Verify the library was created
if [ -f "src/libatari800.a" ]; then
    echo "=== libatari800.a built successfully ==="
    ls -la src/libatari800.a
    echo "Library size: $(stat -c%s src/libatari800.a 2>/dev/null || stat -f%z src/libatari800.a 2>/dev/null || echo 'unknown') bytes"
else
    echo "ERROR: libatari800.a was not created"
    exit 1
fi

echo "=== Full autotools build completed successfully ==="