#!/bin/bash
#
# create-icns.sh - Create macOS .icns icon from PNG source
# 
# This script creates a proper macOS app icon (.icns) from the fujisanlogo.png
# source image, generating all required resolutions for modern macOS versions.
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOURCE_PNG="$PROJECT_ROOT/images/fujisanlogo.png"
OUTPUT_ICNS="$PROJECT_ROOT/Fujisan.icns"
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

# Cleanup function
cleanup() {
    if [[ -n "$TEMP_DIR" && -d "$TEMP_DIR" ]]; then
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

# Verify source image exists
if [[ ! -f "$SOURCE_PNG" ]]; then
    echo_error "Source image not found: $SOURCE_PNG"
    exit 1
fi

echo_info "Creating macOS app icon from: $SOURCE_PNG"
echo_info "Output will be: $OUTPUT_ICNS"

# Create iconset directory
ICONSET_DIR="$TEMP_DIR/Fujisan.iconset"
mkdir -p "$ICONSET_DIR"

# Define icon sizes and their corresponding files
# macOS requires specific naming conventions for .iconset
declare -a ICON_SIZES=(
    "16:icon_16x16.png"
    "32:icon_16x16@2x.png"
    "32:icon_32x32.png"
    "64:icon_32x32@2x.png"
    "128:icon_128x128.png"
    "256:icon_128x128@2x.png"
    "256:icon_256x256.png"
    "512:icon_256x256@2x.png"
    "512:icon_512x512.png"
    "1024:icon_512x512@2x.png"
)

echo_info "Generating icon sizes..."

# Generate each required icon size
for size_info in "${ICON_SIZES[@]}"; do
    size="${size_info%%:*}"
    filename="${size_info##*:}"
    output_path="$ICONSET_DIR/$filename"
    
    echo_info "  Creating ${size}x${size} -> $filename"
    
    # Use sips to resize the image
    if ! sips -z "$size" "$size" "$SOURCE_PNG" --out "$output_path" > /dev/null 2>&1; then
        echo_error "Failed to create icon size: ${size}x${size}"
        exit 1
    fi
done

echo_info "Generated $(ls "$ICONSET_DIR" | wc -l | tr -d ' ') icon files"

# Convert iconset to icns using iconutil
echo_info "Converting iconset to .icns format..."
if ! iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"; then
    echo_error "Failed to create .icns file"
    exit 1
fi

# Verify the created .icns file
if [[ -f "$OUTPUT_ICNS" ]]; then
    file_size=$(stat -f%z "$OUTPUT_ICNS")
    echo_info "Successfully created: $OUTPUT_ICNS"
    echo_info "File size: $file_size bytes"
    
    # Test the icon file with iconutil
    echo_info "Verifying .icns file integrity..."
    if iconutil -V "$OUTPUT_ICNS" > /dev/null 2>&1; then
        echo_info "✓ Icon file verification passed"
    else
        echo_warn "⚠ Icon file verification failed, but file was created"
    fi
else
    echo_error "Failed to create .icns file"
    exit 1
fi

echo_info "✓ macOS app icon creation completed successfully!"
echo_info "Icon file ready for use in CMakeLists.txt: Fujisan.icns"