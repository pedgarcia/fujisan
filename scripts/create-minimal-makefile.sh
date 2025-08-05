#!/bin/bash
# create-minimal-makefile.sh - Create a minimal Makefile for libatari800 when autotools fail

set -e

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

# Create a basic Makefile that compiles the essential files for libatari800
cat > Makefile << 'EOF'
# Minimal Makefile for libatari800
# Generated when autotools are not available

CC = gcc
AR = ar
CFLAGS = -O2 -DHAVE_CONFIG_H -I. -Isrc -DTARGET_LIBATARI800 -DNETSIO
ARFLAGS = rcs

LIBATARI800_OBJS = \
	src/afile.o \
	src/antic.o \
	src/atari.o \
	src/cartridge.o \
	src/cpu.o \
	src/esc.o \
	src/gtia.o \
	src/memory.o \
	src/monitor.o \
	src/pbi.o \
	src/pia.o \
	src/pokey.o \
	src/pokeysnd.o \
	src/sio.o \
	src/sound.o \
	src/statesav.o \
	src/pbi_mio.o \
	src/pbi_bb.o \
	src/pbi_xld.o \
	src/libatari800/api.o \
	src/libatari800/main.o \
	src/libatari800/init.o \
	src/libatari800/input.o \
	src/libatari800/statesav.o

all: src/libatari800.a

src/libatari800.a: $(LIBATARI800_OBJS)
	@echo "=== Creating libatari800.a from $(words $(LIBATARI800_OBJS)) object files ==="
	$(AR) $(ARFLAGS) $@ $^
	@echo "=== libatari800.a created successfully ==="
	@ls -la $@

%.o: %.c
	@echo "Compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIBATARI800_OBJS) src/libatari800.a

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
#define NETSIO 1

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
echo "You can now run 'make' to build libatari800.a"

# Create a marker file to indicate minimal build was used
echo "MINIMAL_BUILD_USED=$(date)" > .minimal-build-marker
echo "Minimal build marker created for debugging"