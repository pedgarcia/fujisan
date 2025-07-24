#!/bin/bash

# Automated patch application script for Fujisan atari800 extensions
# Usage: ./apply-patches.sh /path/to/atari800-src [--reverse]

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCHES_DIR="$SCRIPT_DIR"

# Color output functions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check arguments
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    error "Usage: $0 /path/to/atari800-src [--reverse]"
    exit 1
fi

ATARI_SRC_DIR="$1"
REVERSE_MODE=""

if [ $# -eq 2 ] && [ "$2" = "--reverse" ]; then
    REVERSE_MODE="-R"
    info "Running in REVERSE mode - removing patches"
fi

# Validate atari800 source directory
if [ ! -d "$ATARI_SRC_DIR" ]; then
    error "Atari800 source directory not found: $ATARI_SRC_DIR"
    exit 1
fi

if [ ! -f "$ATARI_SRC_DIR/configure.ac" ]; then
    error "Invalid atari800 source directory (configure.ac not found): $ATARI_SRC_DIR"
    exit 1
fi

# Check for libatari800 target support
if ! grep -q "libatari800" "$ATARI_SRC_DIR/configure.ac"; then
    error "This atari800 version does not support libatari800 target"
    exit 1
fi

info "Atari800 source directory: $ATARI_SRC_DIR"

# Check if patches directory exists
if [ ! -d "$PATCHES_DIR" ]; then
    error "Patches directory not found: $PATCHES_DIR"
    exit 1
fi

# Function to apply a single patch
apply_patch() {
    local patch_file="$1"
    local patch_name=$(basename "$patch_file" .patch)
    
    if [ ! -f "$patch_file" ]; then
        warning "Patch file not found, skipping: $patch_file"
        return 0
    fi
    
    info "Applying patch: $patch_name"
    
    # Test the patch first (dry run)
    if patch --dry-run -p1 $REVERSE_MODE -d "$ATARI_SRC_DIR" < "$patch_file" > /dev/null 2>&1; then
        # Apply the patch for real
        if patch -p1 $REVERSE_MODE -d "$ATARI_SRC_DIR" < "$patch_file"; then
            success "Successfully applied: $patch_name"
        else
            error "Failed to apply: $patch_name"
            return 1
        fi
    else
        error "Patch test failed: $patch_name"
        error "This could mean:"
        error "  - Wrong atari800 version/commit"
        error "  - Patch already applied"
        error "  - Conflicting local modifications"
        return 1
    fi
}

# Record baseline version info
info "Recording atari800 baseline version..."
cd "$ATARI_SRC_DIR"

if [ -d ".git" ]; then
    GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
    GIT_BRANCH=$(git branch --show-current 2>/dev/null || echo "unknown")
    info "Git commit: $GIT_COMMIT"
    info "Git branch: $GIT_BRANCH"
    
    # Update README with version info
    if [ -z "$REVERSE_MODE" ]; then
        sed -i.bak "s/\*\*Atari800 Version\*\*: \[TO BE FILLED[^]]*\]/\*\*Atari800 Version\*\*: $GIT_COMMIT ($GIT_BRANCH)/g" "$PATCHES_DIR/README.md"
    fi
else
    warning "Not a git repository - cannot determine exact version"
fi

# Apply patches in order
info "Applying patches..."

if [ -z "$REVERSE_MODE" ]; then
    info "Installing Fujisan disk activity patches"
else
    info "Removing Fujisan disk activity patches"
fi

# Apply main disk activity API patch
apply_patch "$PATCHES_DIR/disk-activity-api.patch"

# Apply any additional patches
if [ -f "$PATCHES_DIR/Makefile.patch" ]; then
    apply_patch "$PATCHES_DIR/Makefile.patch"
fi

if [ -z "$REVERSE_MODE" ]; then
    success "All patches applied successfully!"
    echo
    info "Next steps:"
    info "1. cd $ATARI_SRC_DIR"
    info "2. ./autogen.sh"
    info "3. ./configure --target=libatari800"
    info "4. make clean && make -j4"
    info "5. Test with: cd $PATCHES_DIR/test && make && ./test-disk-activity"
else
    success "All patches removed successfully!"
    echo
    info "Next steps:"
    info "1. cd $ATARI_SRC_DIR"
    info "2. make clean && make -j4"
fi

echo
info "For more information, see: $PATCHES_DIR/README.md"