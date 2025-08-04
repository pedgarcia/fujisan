#!/bin/bash
# create-ico.sh - Create Windows .ico icon from PNG source

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOURCE_PNG="$PROJECT_ROOT/images/FujisanLogoIcon.png"
OUTPUT_ICO="$PROJECT_ROOT/Fujisan.ico"
TEMP_DIR="$(mktemp -d)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if source PNG exists
if [ ! -f "$SOURCE_PNG" ]; then
    echo_error "Source PNG not found: $SOURCE_PNG"
    exit 1
fi

# Check for ImageMagick
if ! command -v magick >/dev/null 2>&1 && ! command -v convert >/dev/null 2>&1; then
    echo_error "ImageMagick not found. Please install ImageMagick:"
    echo_error "  macOS: brew install imagemagick"
    echo_error "  Linux: sudo apt install imagemagick"
    exit 1
fi

echo_info "Creating Windows app icon from: $SOURCE_PNG"
echo_info "Output will be: $OUTPUT_ICO"

# Cleanup function
cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# Create Windows .ico file with multiple sizes
# Windows .ico format supports: 16, 24, 32, 48, 64, 96, 128, 256 pixels
echo_info "Generating Windows .ico file..."

# Use ImageMagick to create .ico with multiple sizes
if command -v magick >/dev/null 2>&1; then
    # ImageMagick 7.x syntax
    magick "$SOURCE_PNG" \
        \( -clone 0 -resize 16x16 \) \
        \( -clone 0 -resize 24x24 \) \
        \( -clone 0 -resize 32x32 \) \
        \( -clone 0 -resize 48x48 \) \
        \( -clone 0 -resize 64x64 \) \
        \( -clone 0 -resize 96x96 \) \
        \( -clone 0 -resize 128x128 \) \
        \( -clone 0 -resize 256x256 \) \
        -delete 0 "$OUTPUT_ICO"
else
    # ImageMagick 6.x syntax
    convert "$SOURCE_PNG" \
        \( -clone 0 -resize 16x16 \) \
        \( -clone 0 -resize 24x24 \) \
        \( -clone 0 -resize 32x32 \) \
        \( -clone 0 -resize 48x48 \) \
        \( -clone 0 -resize 64x64 \) \
        \( -clone 0 -resize 96x96 \) \
        \( -clone 0 -resize 128x128 \) \
        \( -clone 0 -resize 256x256 \) \
        -delete 0 "$OUTPUT_ICO"
fi

# Verify the created .ico file
if [ -f "$OUTPUT_ICO" ]; then
    FILE_SIZE=$(wc -c < "$OUTPUT_ICO")
    echo_info "Successfully created: $OUTPUT_ICO"
    echo_info "File size: $FILE_SIZE bytes"
else
    echo_error "Failed to create .ico file"
    exit 1
fi

echo_info "✓ Windows app icon creation completed successfully!"
echo_info "Icon file ready for use: Fujisan.ico"