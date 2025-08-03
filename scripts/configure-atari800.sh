#!/bin/bash
# Helper script for configuring atari800 in CMake ExternalProject

set -e

ATARI800_SRC_PATH="$1"

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH not provided"
    exit 1
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

# Configure for libatari800 with universal build support on macOS
echo "Configuring libatari800..."

# Set universal build flags for macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Configuring for universal macOS build (Intel + Apple Silicon)..."
    export CFLAGS="-arch arm64 -arch x86_64 ${CFLAGS:-}"
    export LDFLAGS="-arch arm64 -arch x86_64 ${LDFLAGS:-}"
    export CPPFLAGS="${CPPFLAGS:-}"
fi

./configure --target=libatari800 --enable-netsio

echo "atari800 configuration completed"