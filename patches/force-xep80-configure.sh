#!/bin/bash
# Force enable XEP80 and 80-column hardware after configure is generated

if [ -z "$1" ]; then
    echo "Usage: $0 <atari800-src-path>"
    exit 1
fi

ATARI800_SRC="$1"

# After configure runs, modify config.h to force enable 80-column hardware
if [ -f "$ATARI800_SRC/src/config.h" ]; then
    echo "Forcing XEP80 and 80-column hardware support in config.h..."
    
    # Add defines if they don't exist - only XEP80 now
    if ! grep -q "define XEP80_EMULATION" "$ATARI800_SRC/src/config.h"; then
        echo "#define XEP80_EMULATION 1" >> "$ATARI800_SRC/src/config.h"
    fi
    
    # Comment out the others since we only support XEP80
    # if ! grep -q "define AF80" "$ATARI800_SRC/src/config.h"; then
    #     echo "#define AF80 1" >> "$ATARI800_SRC/src/config.h"
    # fi
    
    # if ! grep -q "define BIT3" "$ATARI800_SRC/src/config.h"; then
    #     echo "#define BIT3 1" >> "$ATARI800_SRC/src/config.h"
    # fi
    
    # if ! grep -q "define PROTO80" "$ATARI800_SRC/src/config.h"; then
    #     echo "#define PROTO80 1" >> "$ATARI800_SRC/src/config.h"
    # fi
    
    echo "✓ 80-column hardware support forcefully enabled"
fi

# Compile the 80-column object files manually since Makefile doesn't include them
if [ -f "$ATARI800_SRC/src/Makefile" ]; then
    echo "Compiling 80-column object files..."
    cd "$ATARI800_SRC/src"
    
    # Get compiler flags from Makefile
    CC=$(grep "^CC = " Makefile | cut -d'=' -f2- | xargs)
    CFLAGS=$(grep "^CFLAGS = " Makefile | cut -d'=' -f2- | xargs)
    CPPFLAGS=$(grep "^CPPFLAGS = " Makefile | cut -d'=' -f2- | xargs)
    DEFS=$(grep "^DEFS = " Makefile | cut -d'=' -f2- | xargs)
    
    # Compile XEP80 files with proper include path
    echo "Compiling xep80.o..."
    $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 xep80.c -o xep80.o
    
    echo "Compiling xep80_fonts.o..."
    $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 xep80_fonts.c -o xep80_fonts.o
    
    # Only compile XEP80 now - skip other 80-column devices
    # if [ -f "af80.c" ]; then
    #     echo "Compiling af80.o..."
    #     $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 af80.c -o af80.o
    # fi
    
    # if [ -f "af80_fonts.c" ]; then
    #     echo "Compiling af80_fonts.o..."
    #     $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 af80_fonts.c -o af80_fonts.o
    # fi
    
    # if [ -f "bit3.c" ]; then
    #     echo "Compiling bit3.o..."
    #     $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 bit3.c -o bit3.o
    # fi
    
    # if [ -f "pbi_proto80.c" ]; then
    #     echo "Compiling pbi_proto80.o..."
    #     $CC -c $CFLAGS $CPPFLAGS $DEFS -I. -I./libatari800 pbi_proto80.c -o pbi_proto80.o
    # fi
    
    # Now add them to libatari800.a
    echo "Adding 80-column objects to libatari800.a..."
    ar r libatari800.a xep80.o xep80_fonts.o 2>/dev/null
    
    if [ -f "af80.o" ]; then
        ar r libatari800.a af80.o 2>/dev/null
    fi
    
    if [ -f "af80_fonts.o" ]; then
        ar r libatari800.a af80_fonts.o 2>/dev/null
    fi
    
    if [ -f "bit3.o" ]; then
        ar r libatari800.a bit3.o 2>/dev/null
    fi
    
    if [ -f "pbi_proto80.o" ]; then
        ar r libatari800.a pbi_proto80.o 2>/dev/null
    fi
    
    ranlib libatari800.a
    
    echo "✓ 80-column object files compiled and added to library"
fi