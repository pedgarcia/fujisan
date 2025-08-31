#!/bin/bash
#
# build-libatari800.sh - Note about libatari800 build integration
#
# This script is no longer needed as libatari800 is now built automatically
# by CMake using ExternalProject. This script is kept for reference.
#

set -e

# Load configuration if not already loaded
if [[ -z "$PROJECT_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    source "$(dirname "$SCRIPT_DIR")/macos-config.sh"
fi

echo_info "libatari800 integration with CMake ExternalProject"
echo_info ""
echo_info "As of the latest update, libatari800 is built automatically"
echo_info "by CMake using ExternalProject during the main Fujisan build."
echo_info ""
echo_info "The CMake build process will:"
echo_info "1. Download atari800 source from GitHub"
echo_info "2. Apply Fujisan patches automatically"
echo_info "3. Configure and build libatari800.a"
echo_info "4. Make it available for linking with Fujisan"
echo_info ""
echo_info "No separate libatari800 build step is required."
echo_info "Just run: cmake --build $BUILD_DIR"
echo_info ""

# Check if libatari800 has already been built by CMake
ATARI800_LIB_PATH="$BUILD_DIR/atari800-build/src/libatari800.a"
if [[ -f "$ATARI800_LIB_PATH" ]]; then
    LIB_SIZE=$(stat -f%z "$ATARI800_LIB_PATH")
    echo_info "✓ libatari800 already built by CMake ($LIB_SIZE bytes)"
    echo_info "Library location: $ATARI800_LIB_PATH"
else
    echo_info "libatari800 will be built during CMake build process"
fi