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
# Note: rdevice excluded on Windows due to network dependency requirements
DESIRED_SOURCES="
afile antic atari cartridge cpu esc gtia memory monitor pbi pia pokey pokeysnd
sio sound statesav pbi_mio pbi_bb pbi_xld mzpokeysnd votraxsnd votrax pbi_scsi
rtime cassette compfile cfg log util colours screen input binload devices
img_tape remez lib artifact ide sysrom videomode
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

# Always include libatari800 API files and SDL platform implementation
EXISTING_OBJS="$EXISTING_OBJS src/libatari800/api.o src/libatari800/main.o src/libatari800/init.o src/libatari800/input.o src/libatari800/statesav.o"

# Create minimal Windows-compatible platform implementation
echo "Creating minimal Windows platform implementation..."
cat > src/platform_minimal.c << 'PLATFORM_EOF'
/* Minimal Windows-compatible platform implementation for libatari800 */
/* Only provides functions not already implemented in libatari800/main.c and libatari800/input.c */

#include "config.h"
#include "platform.h"
#include "videomode.h"
#include "log.h"
#include "atari.h"
#include "memory.h"
#include "sound.h"
#include "input.h"
#include "akey.h"

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <stdio.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* Platform initialization - only ConfigInit which is not in libatari800/main.c */
void PLATFORM_ConfigInit(void) {
    /* Minimal platform configuration initialization */
}

int PLATFORM_Exit(int run_monitor) {
    /* Clean shutdown - no special handling needed */
    return run_monitor;
}

void PLATFORM_DisplayScreen(void) {
    /* No display output in minimal mode */
}

/* Video mode support functions */
int PLATFORM_SupportsVideomode(VIDEOMODE_MODE_t mode, int windowed, int rotate) {
    return TRUE; /* Accept all video modes */
}

void PLATFORM_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate) {
    Log_print("Video mode set to %dx%d (mode %d, windowed %d)", res->width, res->height, mode, windowed);
}

VIDEOMODE_resolution_t *PLATFORM_DesktopResolution(void) {
    static VIDEOMODE_resolution_t res = {1024, 768};
    return &res;
}

int PLATFORM_WindowMaximised(void) {
    return FALSE; /* Window never maximized in minimal mode */
}

VIDEOMODE_resolution_t *PLATFORM_AvailableResolutions(unsigned int *size) {
    static VIDEOMODE_resolution_t resolutions[] = {
        {640, 480}, {800, 600}, {1024, 768}, {1280, 1024}, {1920, 1080}
    };
    *size = 5;
    return resolutions;
}

/* Palette and pixel format support */
void PLATFORM_PaletteUpdate(void) {
    /* No palette update needed in minimal mode */
}

void PLATFORM_GetPixelFormat(PLATFORM_pixel_format_t *format) {
    format->bpp = 32;
    format->rmask = 0x00FF0000;
    format->gmask = 0x0000FF00;
    format->bmask = 0x000000FF;
}

void PLATFORM_MapRGB(void *dest, int const *palette, int size) {
    /* Minimal RGB mapping - could be enhanced later */
    int *output = (int *)dest;
    int i;
    for (i = 0; i < size; i++) {
        output[i] = palette[i]; /* Direct copy for now */
    }
}

/* Timing functions */
void PLATFORM_Sleep(double s) {
#ifdef _WIN32
    Sleep((DWORD)(s * 1000.0));
#else
    usleep((unsigned int)(s * 1000000.0));
#endif
}

double PLATFORM_Time(void) {
#ifdef _WIN32
    return (double)GetTickCount() / 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

/* Sound system - minimal implementations */
int PLATFORM_SoundSetup(Sound_setup_t *setup) {
    Log_print("Sound system initialized (minimal mode - no audio output)");
    return TRUE; /* Pretend sound works */
}

void PLATFORM_SoundExit(void) {
    /* No sound cleanup needed */
}

void PLATFORM_SoundPause(void) {
    /* No-op */
}

void PLATFORM_SoundContinue(void) {
    /* No-op */
}

/* PLATFORM_SoundLock and PLATFORM_SoundUnlock are already defined as macros in platform.h */

/* SDL-specific functions (minimal stubs) */
int PLATFORM_IsKbdJoystickEnabled(int num) {
    return FALSE;
}

void PLATFORM_ToggleKbdJoystickEnabled(int num) {
    /* No-op */
}

int PLATFORM_GetRawKey(void) {
    return 0;
}

/* Joystick configuration stubs */
void PLATFORM_SetJoystickKey(int joystick, int direction, int value) {
    /* No-op */  
}

void PLATFORM_GetJoystickKeyName(int joystick, int direction, char *buffer, int bufsize) {
    snprintf(buffer, bufsize, "None");
}
PLATFORM_EOF

# Add the minimal platform implementation
EXISTING_OBJS="$EXISTING_OBJS src/platform_minimal.o"

echo "Will compile these object files: $EXISTING_OBJS"

# Check for additional platform-specific files
echo "Checking for platform-specific source files..."
PLATFORM_FILES="platform"
for plat_file in $PLATFORM_FILES; do
    if [ -f "src/${plat_file}.c" ]; then
        echo "Found platform file: src/${plat_file}.c"
        EXISTING_OBJS="$EXISTING_OBJS src/${plat_file}.o"
    else
        echo "Platform file not found: src/${plat_file}.c"
    fi
done

# Skip DOS platform files for Windows builds (they require DOS-specific headers like go32.h)

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
#define SUPPORTS_CHANGE_VIDEOMODE 1
#define SUPPORTS_PLATFORM_PALETTEUPDATE 1
#define SUPPORTS_PLATFORM_SLEEP 1
#define SUPPORTS_PLATFORM_TIME 1
#define GUI_SDL 1
#define PLATFORM_MAP_PALETTE 1
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
#define R_NETWORK 1
#define BUFFERED_LOG 1
#define CYCLE_EXACT 1
#define PAGED_ATTRIB 1

/* Disable features that require additional dependencies */
#define NO_SIMPLE_MENU 1

#endif /* CONFIG_H */
EOF

echo "Created minimal Makefile and config.h for libatari800"

# Remove any duplicated API functions that are already present
if grep -q "libatari800_mount_disk_image.*diskno" src/libatari800/api.c 2>/dev/null; then
    echo "Removing duplicate API functions from api.c..."
    # Remove the duplicated section that was added previously
    head -n 495 src/libatari800/api.c > src/libatari800/api.c.tmp
    mv src/libatari800/api.c.tmp src/libatari800/api.c
fi

# Only add genuinely missing functions that Fujisan needs
if ! grep -q "libatari800_set_disk_activity_callback" src/libatari800/api.c 2>/dev/null; then
    echo "Adding only essential missing Fujisan API functions to api.c..."
    cat >> src/libatari800/api.c << 'API_EOF'

/* Additional Fujisan-specific API functions */
static void (*disk_activity_callback)(int drive, int operation) = NULL;

void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation)) {
    disk_activity_callback = callback;
}

void libatari800_dismount_disk_image(int diskno) {
    if (diskno >= 1 && diskno <= 8) {
        /* Implementation for dismounting disk */
    }
}

void libatari800_disable_drive(int diskno) {
    if (diskno >= 1 && diskno <= 8) {
        /* Implementation for disabling drive */
    }
}

void libatari800_enable_drive(int diskno) {
    if (diskno >= 1 && diskno <= 8) {
        /* Implementation for enabling drive */
    }
}

int libatari800_get_sio_patch_enabled(void) {
    return 1; /* SIO patch enabled */
}

int libatari800_get_cartridge_enabled(void) {
    return 0; /* No cartridge by default */
}

void libatari800_set_cartridge_enabled(int enabled) {
    /* Implementation for cartridge control */
}
API_EOF
fi

# Add function declarations only for the new functions
if ! grep -q "libatari800_set_disk_activity_callback" src/libatari800/libatari800.h 2>/dev/null; then
    echo "Adding function declarations for new API functions to libatari800.h..."
    cat >> src/libatari800/libatari800.h << 'HEADER_EOF'

/* Additional Fujisan-specific API function declarations */
void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation));
void libatari800_dismount_disk_image(int diskno);
void libatari800_disable_drive(int diskno);
void libatari800_enable_drive(int diskno);
int libatari800_get_sio_patch_enabled(void);
int libatari800_get_cartridge_enabled(void);
void libatari800_set_cartridge_enabled(int enabled);
HEADER_EOF
fi

echo "You can now run 'make' to build libatari800.a"

# Create a marker file to indicate minimal build was used
echo "MINIMAL_BUILD_USED=$(date)" > .minimal-build-marker
echo "Minimal build marker created for debugging"

