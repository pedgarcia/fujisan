#!/bin/bash
# Helper script for configuring atari800 in CMake ExternalProject

set -e

ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
fi

# Convert Windows paths to Unix paths in MSYS2 environment
if [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]]; then
    # Running in MSYS2 - convert Windows path to Unix path
    ATARI800_SRC_PATH=$(cygpath -u "$ATARI800_SRC_PATH" 2>/dev/null || echo "$ATARI800_SRC_PATH")
    echo "MSYS2 detected - converted path: $ATARI800_SRC_PATH"
fi

if [ ! -d "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH directory does not exist: $ATARI800_SRC_PATH"
    exit 1
fi

echo "Configuring atari800 at: $ATARI800_SRC_PATH"

# Set environment variable for patches
export ATARI800_SRC_PATH

# Change to source directory
cd "$ATARI800_SRC_PATH"

# Apply patches if they exist
if [ -d "fujisan-patches" ] && [ -f "fujisan-patches/apply-patches.sh" ]; then
    echo "Applying Fujisan patches..."
    
    # Configure git identity for patch application (needed in CI environments)
    if [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]] || [[ "$CI" == "true" ]]; then
        if ! git config user.email >/dev/null 2>&1; then
            echo "Configuring git identity for patch application..."
            git config user.email "ci@fujisan.build"
            git config user.name "Fujisan CI"
        fi
    fi
    
    cd fujisan-patches
    chmod +x apply-patches.sh
    ./apply-patches.sh
    cd "$ATARI800_SRC_PATH"
else
    echo "Warning: Fujisan patches not found"
fi

# Generate configure script if needed
if [ ! -f "configure" ]; then
    echo "Generating configure script..."
    ./autogen.sh
fi

# Configure for libatari800 with platform-specific settings
echo "Configuring libatari800..."

# Set platform-specific build flags
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Configuring for universal macOS build (Intel + Apple Silicon)..."
    export CFLAGS="-arch arm64 -arch x86_64 ${CFLAGS:-}"
    export LDFLAGS="-arch arm64 -arch x86_64 ${LDFLAGS:-}"
    export CPPFLAGS="${CPPFLAGS:-}"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]]; then
    echo "Configuring for Windows build with MSYS2/MinGW..."
    # Ensure we're using MinGW-w64 compiler
    export CC=gcc
    export CXX=g++
    # Set basic optimization flags
    export CFLAGS="-O2 ${CFLAGS:-}"
    export LDFLAGS="${LDFLAGS:-}"
    export CPPFLAGS="${CPPFLAGS:-}"
fi

./configure --target=libatari800 --enable-netsio

echo "atari800 configuration completed"