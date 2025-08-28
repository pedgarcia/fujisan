#!/bin/bash
# Force enable XEP80 after configure is generated - handles fat binaries properly

if [ -z "$1" ]; then
    echo "Usage: $0 <atari800-src-path>"
    exit 1
fi

ATARI800_SRC="$1"
cd "$ATARI800_SRC/src" || exit 1

# After configure runs, modify config.h to force enable XEP80
if [ -f "config.h" ]; then
    echo "Forcing XEP80 support in config.h..."
    
    # Add define if it doesn't exist
    if ! grep -q "define XEP80_EMULATION" "config.h"; then
        echo "#define XEP80_EMULATION 1" >> "config.h"
    fi
    
    echo "✓ XEP80 support enabled in config.h"
fi

# Compile the XEP80 object files manually since Makefile doesn't include them
if [ -f "Makefile" ]; then
    echo "Compiling XEP80 object files..."
    
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
    
    # Now properly add them to the fat libatari800.a
    echo "Adding XEP80 objects to libatari800.a..."
    
    # Check if libatari800.a is a fat file
    if lipo -info libatari800.a 2>/dev/null | grep -q "are:"; then
        echo "Handling fat library..."
        
        # Extract each architecture
        lipo -thin arm64 libatari800.a -output libatari800_arm64.a 2>/dev/null || true
        lipo -thin x86_64 libatari800.a -output libatari800_x86_64.a 2>/dev/null || true
        
        # Extract XEP80 objects for each architecture
        if [ -f libatari800_arm64.a ]; then
            lipo -thin arm64 xep80.o -output xep80_arm64.o 2>/dev/null || cp xep80.o xep80_arm64.o
            lipo -thin arm64 xep80_fonts.o -output xep80_fonts_arm64.o 2>/dev/null || cp xep80_fonts.o xep80_fonts_arm64.o
            ar r libatari800_arm64.a xep80_arm64.o xep80_fonts_arm64.o
            ranlib libatari800_arm64.a
            rm -f xep80_arm64.o xep80_fonts_arm64.o
        fi
        
        if [ -f libatari800_x86_64.a ]; then
            lipo -thin x86_64 xep80.o -output xep80_x86_64.o 2>/dev/null || cp xep80.o xep80_x86_64.o
            lipo -thin x86_64 xep80_fonts.o -output xep80_fonts_x86_64.o 2>/dev/null || cp xep80_fonts.o xep80_fonts_x86_64.o
            ar r libatari800_x86_64.a xep80_x86_64.o xep80_fonts_x86_64.o
            ranlib libatari800_x86_64.a
            rm -f xep80_x86_64.o xep80_fonts_x86_64.o
        fi
        
        # Recreate fat library
        if [ -f libatari800_arm64.a ] && [ -f libatari800_x86_64.a ]; then
            lipo -create libatari800_arm64.a libatari800_x86_64.a -output libatari800.a
            rm -f libatari800_arm64.a libatari800_x86_64.a
        elif [ -f libatari800_arm64.a ]; then
            mv libatari800_arm64.a libatari800.a
        elif [ -f libatari800_x86_64.a ]; then
            mv libatari800_x86_64.a libatari800.a
        fi
    else
        # Simple single-architecture library
        echo "Handling single-architecture library..."
        ar r libatari800.a xep80.o xep80_fonts.o
        ranlib libatari800.a
    fi
    
    echo "✓ XEP80 object files compiled and added to library"
fi