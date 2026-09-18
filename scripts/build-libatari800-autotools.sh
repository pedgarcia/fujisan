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

# Set cross-compilation environment variables BEFORE applying patches
# so that the patch script can detect Windows builds via CC variable
export CC="x86_64-w64-mingw32-gcc"
export CXX="x86_64-w64-mingw32-g++"
export AR="x86_64-w64-mingw32-ar"
export RANLIB="x86_64-w64-mingw32-ranlib"
export STRIP="x86_64-w64-mingw32-strip"
export PKG_CONFIG_PATH="/usr/x86_64-w64-mingw32/lib/pkgconfig"

# Ensure Fujisan patches are applied for required API functions
if [ -d "fujisan-patches" ] && [ -f "fujisan-patches/apply-patches.sh" ]; then
    echo "Applying Fujisan patches for API functions..."
    cd fujisan-patches
    chmod +x apply-patches.sh
    # Set ATARI800_SRC_PATH for the patch script (we're already in it, so use parent dir)
    ATARI800_SRC_PATH="$(dirname $(pwd))" ./apply-patches.sh || echo "Patches may already be applied"
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

# Configure with appropriate flags for Windows cross-compilation
./configure \
    --host=x86_64-w64-mingw32 \
    --target=libatari800 \
    --disable-sdltest \
    --without-sdl \
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

# Enable per-instruction PC breakpoints (configure --disable-monitorbreak and
# the libatari800 target preset force them off) and provide
# libatari800_debug_step_instruction(). This mirrors what
# scripts/configure-atari800.sh does for the Unix/macOS build; without it the
# Fujisan link fails with "undefined reference to
# libatari800_debug_step_instruction" and PC breakpoints never fire.
sed -i.bak 's|/\* #undef MONITOR_BREAK \*/|#define MONITOR_BREAK 1|' src/config.h
sed -i.bak 's|/\* #undef MONITOR_BREAKPOINTS \*/|#define MONITOR_BREAKPOINTS 1|' src/config.h

if ! grep -q 'libatari800_debug_step_instruction' src/libatari800/api.c; then
    cat >> src/libatari800/api.c <<'FUJISAN_STEP_EOF'

/* Execute exactly one 6502 instruction (debugger single-step).
 *
 * MONITOR_break_step makes CPU_GO() stop after a single instruction, and the
 * core skips the PC breakpoint table while it is set -- exactly what a debugger
 * needs to step off a breakpoint. The stop arrives as DO_BREAK -> ENTER_MONITOR
 * -> Atari800_Exit(), which in libatari800 would terminate the process, so arm
 * the same longjmp landing pad libatari800_next_frame() uses (see
 * PLATFORM_Exit() in exit.c) for the duration of the step. */
void libatari800_debug_step_instruction(void)
{
#if defined(MONITOR_BREAK) && defined(HAVE_SETJMP)
	if (setjmp(libatari800_monitor_jmp) == 0) {
		libatari800_monitor_active = 1;
		MONITOR_break_step = TRUE;
		/* CPU_GO() runs while ANTIC_xpos < limit, so the limit must be
		   relative to where we currently are in the scanline. A fixed
		   limit (e.g. CPU_GO(20)) silently executes nothing whenever the
		   CPU is already past that cycle, making the "step" a no-op.
		   MONITOR_break_step stops us after one instruction regardless,
		   so a small margin is enough. */
		CPU_GO(ANTIC_xpos + 8);
	}
	libatari800_monitor_active = 0;
	MONITOR_break_step = FALSE;
#endif
}
FUJISAN_STEP_EOF
    echo "injected libatari800_debug_step_instruction() into api.c"
fi
if ! grep -q 'libatari800_debug_step_instruction' src/libatari800/libatari800.h; then
    sed -i.bak 's|#endif /\* LIBATARI800_H_ \*/|/* Execute exactly one 6502 instruction (debugger single-step). */\nvoid libatari800_debug_step_instruction(void);\n\n#endif /* LIBATARI800_H_ */|' src/libatari800/libatari800.h
    echo "declared libatari800_debug_step_instruction() in libatari800.h"
fi

# Apply Windows-specific fixes
echo "Applying Windows-specific fixes..."

# On MinGW x64, ULONG is 32-bit (LLP64). Do NOT redefine ULONG; that breaks
# Windows API (VerSetConditionMask) and causes "conflicting types" (ANTIC_VideoMemset).
# Ensure libatari800.h does not define ULONG on Windows (use Windows' typedef).

if ! grep -q "defined(_WIN32)" src/libatari800/libatari800.h; then
    echo "Patching libatari800.h: ensure Windows ULONG is available..."
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
fi

# 0002 patch removes ANTIC_lookup_gtia9/11 from antic.c, but antic.h still
# declares them extern and antic.c uses them. Restore the definitions if missing.
if ! grep -q '^ULONG ANTIC_lookup_gtia9\[16\];' src/antic.c; then
    echo "Restoring ANTIC_lookup_gtia arrays in antic.c..."
    sed -i.bak '/static UWORD lookup2\[256\];/a\
ULONG ANTIC_lookup_gtia9[16];\
ULONG ANTIC_lookup_gtia11[16];
' src/antic.c
fi

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