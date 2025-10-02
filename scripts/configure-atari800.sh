#!/bin/bash
# Helper script for configuring atari800 in CMake ExternalProject

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
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
    
    # Try different approaches to generate configure script
    if [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]] || [[ "$CI" == "true" ]]; then
        echo "Windows CI detected - trying alternative configure generation..."
        
        # Check if we can find a distributed configure script
        if [ -f "configure.ac" ] || [ -f "configure.in" ]; then
            echo "Found autotools files, attempting manual configure generation..."
            
            # Try to run autotools manually if available
            if command -v autoreconf >/dev/null 2>&1; then
                echo "Using autoreconf..."
                autoreconf -fiv
            elif command -v autoconf >/dev/null 2>&1 && command -v aclocal >/dev/null 2>&1; then
                echo "Using autoconf/aclocal manually..."
                aclocal && autoheader && autoconf && automake --add-missing --copy || true
            else
                echo "ERROR: No autotools available - using minimal Makefile fallback"
                echo "Creating minimal build system for libatari800..."
                "$SCRIPT_DIR/create-minimal-makefile.sh" "$ATARI800_SRC_PATH"
                
                # Skip the ./configure step since we're using direct Makefile
                echo "Skipping configure step - using minimal Makefile approach"
                echo "atari800 configuration completed (minimal build)"
                exit 0
            fi
        else
            echo "ERROR: No configure.ac or configure.in found"
            exit 1
        fi
    else
        # Unix/macOS - use autogen.sh as normal
        ./autogen.sh
    fi
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
    echo "MSYSTEM: $MSYSTEM"
    echo "Current PATH: $PATH"
    echo "Checking for autotools..."

    # Initialize MSYS2 environment if needed
    if [[ "$MSYSTEM" == "MSYS" ]] && [[ -f "/etc/profile" ]]; then
        echo "Sourcing MSYS2 profile..."
        source /etc/profile || true
    fi

    which autoconf || echo "autoconf not found"
    which aclocal || echo "aclocal not found"
    which automake || echo "automake not found"
    which autoreconf || echo "autoreconf not found"

    # Ensure MinGW-w64 tools are in PATH
    export PATH="/mingw64/bin:/usr/bin:$PATH"
    echo "Updated PATH for MinGW-w64: $PATH"
    which gcc || echo "gcc not found"
    which g++ || echo "g++ not found"

    # Ensure we're using MinGW-w64 compiler
    export CC=gcc
    export CXX=g++
    # Set basic optimization flags
    export CFLAGS="-O2 ${CFLAGS:-}"
    export LDFLAGS="${LDFLAGS:-}"
    export CPPFLAGS="${CPPFLAGS:-}"
else
    # Linux: Set aggressive optimization flags for performance
    echo "Configuring for Linux build with optimizations..."
    echo "Compiler: $(gcc --version | head -1 2>/dev/null || echo 'gcc not found')"

    # Use -O3 for maximum optimization, -march=native for CPU-specific optimizations
    # Note: In containers, we use the container's native arch (specified by --platform)
    export CFLAGS="-O3 -march=native -mtune=native ${CFLAGS:-}"
    export LDFLAGS="${LDFLAGS:-}"
    export CPPFLAGS="${CPPFLAGS:-}"

    echo "CFLAGS: $CFLAGS"
fi

# Debug environment variables
echo "=== Environment Detection ==="
echo "OSTYPE: $OSTYPE"
echo "MSYSTEM: $MSYSTEM"
echo "OS: $OS" 
echo "RUNNER_OS: $RUNNER_OS"
echo "CI: $CI"
echo "pwd: $(pwd)"
echo "uname: $(uname -a 2>/dev/null || echo 'uname not available')"
echo "=========================="

# Configure with Windows-specific settings if needed
# Check multiple ways to detect Windows environment
if [[ "$OSTYPE" == "msys" ]] || [[ "$MSYSTEM" != "" ]] || [[ "$OS" == "Windows_NT" ]] || [[ "$RUNNER_OS" == "Windows" ]] || [[ "$CI" == "true" && $(uname -s 2>/dev/null || echo '') == *"MSYS"* ]]; then
    echo "Windows detected - using minimal Makefile approach to avoid autotools issues"
    echo "Detection triggered by: OSTYPE=$OSTYPE, MSYSTEM=$MSYSTEM, OS=$OS, RUNNER_OS=$RUNNER_OS"
    echo "Creating minimal build system for libatari800..."
    "$SCRIPT_DIR/create-minimal-makefile.sh" "$ATARI800_SRC_PATH"
    echo "Skipping configure step - using minimal Makefile approach"
    echo "atari800 configuration completed (minimal build)"
    exit 0
else
    # Unix/macOS: Use full configuration
    ./configure --target=libatari800 --enable-netsio
fi

echo "atari800 configuration completed"

# Apply inline patches if needed for missing functions
INLINE_PATCH="${PATCHES_DIR}/patch-libatari800-inline.sh"
if [ -f "$INLINE_PATCH" ]; then
    echo "Applying inline patches for disk management functions..."
    bash "$INLINE_PATCH" "$ATARI800_SRC_PATH" || echo "Inline patch may have already been applied"
fi