#!/bin/bash
# Build libatari800 with FujiNet/NetSIO support for Windows cross-compilation
set -e

echo "=== Building libatari800 with FujiNet/NetSIO support ==="
ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
fi

cd "$ATARI800_SRC_PATH"
echo "Building in: $(pwd)"

# Apply Fujisan patches if they exist and haven't been applied
if [ -d "fujisan-patches" ]; then
    echo "=== Applying Fujisan patches ==="
    if git status >/dev/null 2>&1; then
        # We're in a git repository - use git am
        for patch in fujisan-patches/*.patch; do
            if [ -f "$patch" ]; then
                echo "Applying patch: $patch"
                git am --ignore-whitespace "$patch" 2>/dev/null || {
                    echo "Patch may already be applied, continuing..."
                    git am --abort 2>/dev/null || true
                }
            fi
        done
    else
        # Not a git repo - use traditional patch command
        for patch in fujisan-patches/*.patch; do
            if [ -f "$patch" ]; then
                echo "Applying patch: $patch"
                patch -p1 < "$patch" || echo "Patch may already be applied, continuing..."
            fi
        done
    fi
else
    echo "No Fujisan patches found"
fi

# Generate configure script if it doesn't exist
if [ ! -f "configure" ]; then
    echo "=== Generating configure script ==="
    if [ -f "autogen.sh" ]; then
        ./autogen.sh
    else
        echo "ERROR: No configure script and no autogen.sh found"
        exit 1
    fi
fi

# Configure with FujiNet/NetSIO support
echo "=== Configuring atari800 with NetSIO support ==="
./configure \
    --host=x86_64-w64-mingw32 \
    --target=libatari800 \
    --enable-netsio \
    --enable-rnetwork \
    --disable-sdltest \
    --without-readline \
    --without-termcap \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    AR=x86_64-w64-mingw32-ar \
    RANLIB=x86_64-w64-mingw32-ranlib \
    PKG_CONFIG_PATH="" \
    CFLAGS="-O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX" \
    CXXFLAGS="-O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX"

# Apply Windows-specific fixes (same as build-libatari800-autotools.sh)
echo "=== Applying Windows-specific fixes ==="

# On MinGW x64, ULONG is 32-bit (LLP64). Do NOT redefine ULONG; that breaks
# Windows API and causes "conflicting types" for ANTIC_VideoMemset.
# Guard the ULONG define in libatari800.h so it is skipped on Windows.
if ! grep -q "defined(_WIN32)" src/libatari800/libatari800.h; then
    echo "Patching libatari800.h: guard ULONG define for Windows..."
    awk '
    /^#define LIBATARI800_H_$/ {
        print
        print ""
        print "#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__) || defined(HAVE_WINDOWS_H)"
        print "#include <windows.h>"
        print "#endif"
        next
    }
    /^#ifndef ULONG$/ {
        print "#ifndef ULONG"
        print "#if !(defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__) || defined(HAVE_WINDOWS_H))"
        print "/* On Windows, use Windows ULONG; do not define here */"
        print "#include <stdint.h>"
        print "#define ULONG uint32_t"
        print "#endif"
        print "#endif"
        getline; getline; getline  # skip original #include, #define, #endif
        next
    }
    { print }
    ' src/libatari800/libatari800.h > src/libatari800/libatari800.h.new
    mv src/libatari800/libatari800.h.new src/libatari800/libatari800.h
    echo "✓ libatari800.h patched"
fi

# 0002 patch removes ANTIC_lookup_gtia9/11 from antic.c, but antic.h still
# declares them extern and antic.c uses them. Restore the definitions if missing.
if ! grep -q '^ULONG ANTIC_lookup_gtia9\[16\];' src/antic.c; then
    echo "Restoring ANTIC_lookup_gtia arrays in antic.c..."
    sed -i.bak '/static UWORD lookup2\[256\];/a\
ULONG ANTIC_lookup_gtia9[16];\
ULONG ANTIC_lookup_gtia11[16];
' src/antic.c
    echo "✓ ANTIC_lookup_gtia arrays restored"
fi

# Build libatari800.a
echo "=== Building libatari800 with NetSIO ==="
make clean 2>/dev/null || true
make -j4 || make  # Retry single-threaded if parallel fails

# Verify the library was created
if [ -f "src/libatari800.a" ]; then
    echo "=== libatari800.a built successfully with NetSIO support ==="
    ls -la src/libatari800.a
    echo "Library size: $(stat -c%s src/libatari800.a 2>/dev/null || stat -f%z src/libatari800.a 2>/dev/null || echo 'unknown') bytes"
    
    # Check if NetSIO is enabled in config.h
    if grep -q "#define NETSIO 1" src/config.h; then
        echo "✓ NetSIO support confirmed in config.h"
    else
        echo "⚠  NetSIO may not be enabled - check src/config.h"
        grep NETSIO src/config.h || true
    fi
    
    echo "=== Build with NetSIO completed successfully ==="
    exit 0
else
    echo "ERROR: libatari800.a not found after build"
    exit 1
fi