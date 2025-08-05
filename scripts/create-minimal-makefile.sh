#!/bin/bash
# create-minimal-makefile.sh - Create a minimal Makefile for libatari800 when autotools fail

set -e

echo "=== Creating minimal build system for libatari800 ==="
echo "PWD: $(pwd)"
echo "Available files in src/:"
ls -la src/ 2>/dev/null | head -10 || echo "Cannot list src/ directory"

ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
fi

cd "$ATARI800_SRC_PATH"

# Ensure MinGW-w64 tools are in PATH for Windows builds
if [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]]; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
    echo "Updated PATH for MinGW-w64: $PATH"
fi

echo "Creating minimal Makefile for libatari800..."
echo "This approach avoids autotools and problematic modules like XEP80"

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

# Include real source files for full Windows functionality
echo "Checking which source files exist..."

# List of source files we want to include (without .o extension)
DESIRED_SOURCES="
afile antic atari cartridge cpu esc gtia memory monitor pbi pia pokey pokeysnd
sio sound statesav pbi_mio pbi_bb pbi_xld mzpokeysnd votraxsnd votrax pbi_scsi
rtime cassette compfile cfg log util colours screen input binload devices
img_tape remez lib artifact ide rdevice sysrom videomode
"

# Check which files actually exist and build the object list
EXISTING_OBJS=""
for src in $DESIRED_SOURCES; do
    if [ -f "src/${src}.c" ]; then
        echo "Found: src/${src}.c"
        EXISTING_OBJS="$EXISTING_OBJS src/${src}.o"
    else
        echo "Missing: src/${src}.c - skipping"
    fi
done

# Always include libatari800 API files and platform stubs
EXISTING_OBJS="$EXISTING_OBJS src/libatari800/api.o src/libatari800/main.o src/libatari800/init.o src/libatari800/input.o src/libatari800/statesav.o src/platform_stubs.o"

echo "Will compile these object files: $EXISTING_OBJS"

# Create platform stubs for missing functions
cat > src/platform_stubs.c << 'PLATFORM_EOF'
/* Platform function stubs for minimal libatari800 build */

#include "videomode.h"
#include "platform.h"

/* Platform video mode stubs */
int PLATFORM_SupportsVideomode(VIDEOMODE_MODE_t mode, int windowed, VIDEOMODE_ROTATE_t rotate) {
    return 1; /* Assume all modes supported */
}

void PLATFORM_SetVideoMode(VIDEOMODE_resolution_t *res, int windowed, VIDEOMODE_MODE_t mode, VIDEOMODE_ROTATE_t rotate) {
    /* Stub - no actual video mode setting */
}

VIDEOMODE_resolution_t* PLATFORM_DesktopResolution(void) {
    static VIDEOMODE_resolution_t res = {640, 480};
    return &res;
}

int PLATFORM_WindowMaximised(void) {
    return 0; /* Never maximized */
}

VIDEOMODE_resolution_t* PLATFORM_AvailableResolutions(int *size) {
    static VIDEOMODE_resolution_t resolutions[] = {{640, 480}, {800, 600}, {1024, 768}};
    *size = 3;
    return resolutions;
}

void PLATFORM_Exit(int code) {
    /* Stub - no platform-specific exit handling */
}
PLATFORM_EOF

# Create a basic Makefile that compiles the essential files for libatari800
cat > Makefile << EOF
# Minimal Makefile for libatari800
# Generated when autotools are not available

CC = gcc
AR = ar
CFLAGS = -O2 -DHAVE_CONFIG_H -I. -Isrc -DTARGET_LIBATARI800
ARFLAGS = rcs

LIBATARI800_OBJS =$EXISTING_OBJS

all: src/libatari800.a

src/libatari800.a: \$(LIBATARI800_OBJS)
	@echo "=== Creating libatari800.a from \$(words \$(LIBATARI800_OBJS)) object files ==="
	\$(AR) \$(ARFLAGS) \$@ \$^
	@echo "=== libatari800.a created successfully ==="
	@ls -la \$@

%.o: %.c
	@echo "Compiling \$< ..."
	\$(CC) \$(CFLAGS) -c \$< -o \$@

clean:
	rm -f \$(LIBATARI800_OBJS) src/libatari800.a

.PHONY: all clean
EOF

# Create a minimal config.h
cat > src/config.h << 'EOF'
/* Minimal config.h for libatari800 */
#ifndef CONFIG_H
#define CONFIG_H

#define PACKAGE_STRING "atari800 4.2.0"
#define PACKAGE_VERSION "4.2.0"
#define VERSION "4.2.0"

#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_DIRENT_H 1

#ifdef _WIN32
#define HAVE_WINDOWS_H 1
#define HAVE_WINSOCK2_H 1
#endif

#define SUPPORTS_PLATFORM_CONFIGINIT 1
#define SUPPORTS_PLATFORM_CONFIGURE 1
#define SUPPORTS_PLATFORM_INITIALISE 1
#define SUPPORTS_PLATFORM_EXIT 1
#define SUPPORTS_SOUND_REINIT 1
#define SOUND_GAIN 4
#define STEREO_SOUND 1
#define CONSOLE_SOUND 1
#define SERIO_SOUND 1
#define CLIP_SOUND 1
#define SYNCHRONIZED_SOUND 1
#define SOUND_INTERPOLATION 1

/* Disable potentially problematic Windows features */
#undef XEP80_EMULATION
#undef AF80
#undef BIT3
#define NONLINEAR_MIXING 1

/* Sound system - enable sound for libatari800 */
#define SOUND 1

/* NetSIO support */
/* NETSIO officially not supported on Windows platform */
#undef NETSIO

/* Screenshot support */
#define HAVE_LIBPNG 1

/* Enable various features */
#define PBI_MIO 1
#define PBI_BB 1
#define PBI_XLD 1
#define IDE 1
#define R_IO_DEVICE 1
#define BUFFERED_LOG 1
#define CYCLE_EXACT 1
#define PAGED_ATTRIB 1

/* Disable features that require additional dependencies */
#define NO_SIMPLE_MENU 1

#endif /* CONFIG_H */
EOF

echo "Created minimal Makefile and config.h for libatari800"

# Add missing Fujisan API functions to api.c if not present
if ! grep -q "libatari800_set_disk_activity_callback" src/libatari800/api.c 2>/dev/null; then
    echo "Adding missing Fujisan API functions to api.c..."
    cat >> src/libatari800/api.c << 'API_EOF'
/* Fujisan-specific API functions for disk management */
static void (*disk_activity_callback)(int drive, int operation) = NULL;

void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation)) {
    disk_activity_callback = callback;
}

void libatari800_dismount_disk_image(int diskno) {
    /* Simple dismount implementation */
    if (diskno >= 1 && diskno <= 8) {
        /* Implementation would go here - for now just placeholder */
    }
}

void libatari800_disable_drive(int diskno) {
    /* Simple disable implementation */
    if (diskno >= 1 && diskno <= 8) {
        /* Implementation would go here - for now just placeholder */
    }
}
API_EOF
fi

# Add missing function declarations to libatari800.h if not present
if ! grep -q "libatari800_set_disk_activity_callback" src/libatari800/libatari800.h 2>/dev/null; then
    echo "Adding missing function declarations to libatari800.h..."
    cat >> src/libatari800/libatari800.h << 'HEADER_EOF'
/* Fujisan-specific API function declarations */
void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation));
void libatari800_dismount_disk_image(int diskno);
void libatari800_disable_drive(int diskno);
HEADER_EOF
fi

echo "You can now run 'make' to build libatari800.a"

# Create a marker file to indicate minimal build was used
echo "MINIMAL_BUILD_USED=$(date)" > .minimal-build-marker
echo "Minimal build marker created for debugging"

