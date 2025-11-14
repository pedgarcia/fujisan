#!/bin/bash
#
# Download FujiNet-PC nightly builds for all supported platforms
# This script downloads the latest ATARI builds and organizes them by platform
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FUJINET_DIR="$PROJECT_ROOT/fujinet"

# GitHub API URL
GITHUB_API="https://api.github.com/repos/FujiNetWIFI/fujinet-firmware/releases"

echo "=== FujiNet-PC Download Script ==="
echo

# Fetch latest nightly release
echo "Fetching latest nightly release..."
API_RESPONSE=$(curl -s "$GITHUB_API")

# Extract tag name from first nightly release (they start with "nightly_fn_pc_")
TAG=$(echo "$API_RESPONSE" | grep -o '"tag_name"[[:space:]]*:[[:space:]]*"nightly_fn_pc_[^"]*"' | head -n 1 | cut -d '"' -f 4)

if [ -z "$TAG" ]; then
    echo "ERROR: Could not find latest nightly release"
    echo "Please check: https://github.com/FujiNetWIFI/fujinet-firmware/releases"
    exit 1
fi

echo "Latest nightly tag: $TAG"
echo

# Base download URL (note: + in tag needs to be URL encoded as %2B)
TAG_ENCODED=$(echo "$TAG" | sed 's/+/%2B/g')
BASE_URL="https://github.com/FujiNetWIFI/fujinet-firmware/releases/download/$TAG_ENCODED"

# Platform configurations: "folder_name|filename_pattern"
PLATFORMS=(
    "macos-arm64|fujinet-pc-ATARI_${TAG#nightly_fn_pc_}_macos-14.tar.gz"
    "macos-x86_64|fujinet-pc-ATARI_${TAG#nightly_fn_pc_}_macos-13.tar.gz"
    "ubuntu-22.04|fujinet-pc-ATARI_${TAG#nightly_fn_pc_}_ubuntu-22.04-amd64.tar.gz"
    "ubuntu-24.04|fujinet-pc-ATARI_${TAG#nightly_fn_pc_}_ubuntu-24.04-amd64.tar.gz"
    "windows-x64|fujinet-pc-ATARI_${TAG#nightly_fn_pc_}_windows-x64.zip"
)

# Create fujinet directory
mkdir -p "$FUJINET_DIR"

# Download and extract each platform
for platform_config in "${PLATFORMS[@]}"; do
    IFS='|' read -r folder filename <<< "$platform_config"

    echo "Downloading $folder..."
    DOWNLOAD_URL="$BASE_URL/$filename"
    DEST_DIR="$FUJINET_DIR/$folder"
    TEMP_FILE="$FUJINET_DIR/$filename"

    # Download
    if ! curl -L -f -o "$TEMP_FILE" "$DOWNLOAD_URL"; then
        echo "WARNING: Failed to download $filename"
        continue
    fi

    # Create destination directory
    mkdir -p "$DEST_DIR"

    # Extract based on file type
    echo "Extracting to $folder..."
    if [[ "$filename" == *.tar.gz ]]; then
        tar -xzf "$TEMP_FILE" -C "$DEST_DIR"
    elif [[ "$filename" == *.zip ]]; then
        unzip -q -o "$TEMP_FILE" -d "$DEST_DIR"
    fi

    # Move contents from subdirectory to platform directory
    # Archives extract into fujinet-pc-ATARI/ or fujinet-pc-APPLE/ subdirectory
    SUBDIR=$(find "$DEST_DIR" -maxdepth 1 -type d -name "fujinet-pc-*" | head -n 1)
    if [[ -d "$SUBDIR" ]]; then
        mv "$SUBDIR"/* "$DEST_DIR/" 2>/dev/null || true
        rmdir "$SUBDIR" 2>/dev/null || true
    fi

    # Clean up archive
    rm -f "$TEMP_FILE"

    echo "✓ $folder complete"
    echo
done

echo "=== Download Complete ==="
echo
echo "FujiNet-PC binaries downloaded to: $FUJINET_DIR"
echo
echo "Directory structure:"
ls -lh "$FUJINET_DIR"
echo

# Show versions
echo "Installed versions:"
for platform_config in "${PLATFORMS[@]}"; do
    IFS='|' read -r folder filename <<< "$platform_config"
    BINARY_PATH="$FUJINET_DIR/$folder/fujinet"
    if [[ "$folder" == "windows-x64" ]]; then
        BINARY_PATH="$FUJINET_DIR/$folder/fujinet.exe"
    fi

    if [ -f "$BINARY_PATH" ]; then
        echo "  $folder: $(basename "$BINARY_PATH") found"
    else
        echo "  $folder: NOT FOUND"
    fi
done

echo
echo "Done!"
