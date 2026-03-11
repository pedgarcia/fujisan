#!/bin/bash
#
# Download Fastbasic release from GitHub and extract for bundling.
# Used when building Fujisan with --build-fastbasic-compiler.
#
# Usage: ./download-fastbasic.sh [version]
#   version: e.g. v4.7 (default)
#
# Output: PROJECT_ROOT/fastbasic/macosx/ (full contents for Mac)
#         PROJECT_ROOT/fastbasic/linux64/ (for Linux amd64)
#         PROJECT_ROOT/fastbasic/win32/   (for Windows)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FASTBASIC_VERSION="${1:-v4.7}"
FASTBASIC_DIR="$PROJECT_ROOT/fastbasic"
BASE_URL="https://github.com/dmsc/fastbasic/releases/download/${FASTBASIC_VERSION}"

echo "=== Fastbasic Download Script ==="
echo "Version: $FASTBASIC_VERSION"
echo ""

mkdir -p "$FASTBASIC_DIR"
cd "$FASTBASIC_DIR"

download_and_extract() {
    local name="$1"
    local zip="$2"
    local dest="$3"
    echo "Downloading $name..."
    if ! curl -sL -f -o "$zip" "$BASE_URL/$zip"; then
        echo "ERROR: Failed to download $zip"
        return 1
    fi
    rm -rf "$dest"
    mkdir -p "$dest"
    unzip -q -o "$zip" -d "$dest"
    rm -f "$zip"
    echo "  -> $dest"
}

# Mac (same zip for both arm64 and x86_64; runs under Rosetta on arm64)
# GitHub asset names include the "v", e.g. fastbasic-v4.7-macosx.zip
ZIP_MAC="fastbasic-${FASTBASIC_VERSION}-macosx.zip"
if [[ ! -d "$FASTBASIC_DIR/macosx" ]] || [[ ! -f "$FASTBASIC_DIR/macosx/fastbasic" ]]; then
    download_and_extract "Mac" "$ZIP_MAC" "macosx"
else
    echo "Mac fastbasic already present at fastbasic/macosx/"
fi

# Linux x86_64
ZIP_LINUX="fastbasic-${FASTBASIC_VERSION}-linux64.zip"
if [[ ! -d "$FASTBASIC_DIR/linux64" ]] || [[ ! -f "$FASTBASIC_DIR/linux64/fastbasic" ]]; then
    download_and_extract "Linux x86_64" "$ZIP_LINUX" "linux64"
else
    echo "Linux fastbasic already present at fastbasic/linux64/"
fi

# Windows
ZIP_WIN="fastbasic-${FASTBASIC_VERSION}-win32.zip"
if [[ ! -d "$FASTBASIC_DIR/win32" ]] || [[ ! -f "$FASTBASIC_DIR/win32/fastbasic.exe" ]]; then
    download_and_extract "Windows" "$ZIP_WIN" "win32"
else
    echo "Windows fastbasic already present at fastbasic/win32/"
fi

echo ""
echo "Done. Use fastbasic/macosx/ for macOS app bundle, fastbasic/linux64/ for Linux, fastbasic/win32/ for Windows."
