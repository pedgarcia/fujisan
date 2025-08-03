#!/bin/bash
#
# macos-config.sh - Configuration for macOS Fujisan build process
#
# This file contains all configurable settings for building, signing, and
# packaging Fujisan for macOS. No personal paths or sensitive data should
# be stored here - use environment variables for those.
#

# ============================================================================
# PROJECT CONFIGURATION
# ============================================================================

# Project information
PROJECT_NAME="Fujisan"
BUNDLE_ID="com.8bitrelics.fujisan"
APP_CATEGORY="public.app-category.entertainment"

# Version extraction from git or manual override
if [[ -n "$FUJISAN_VERSION" ]]; then
    PROJECT_VERSION="$FUJISAN_VERSION"
elif git describe --tags --exact-match 2>/dev/null; then
    PROJECT_VERSION=$(git describe --tags --exact-match)
elif git describe --tags 2>/dev/null; then
    PROJECT_VERSION=$(git describe --tags)
else
    PROJECT_VERSION="1.0.0-dev"
fi

# Remove 'v' prefix if present
PROJECT_VERSION="${PROJECT_VERSION#v}"

echo "Project Version: $PROJECT_VERSION"

# ============================================================================
# PATHS CONFIGURATION (Auto-detected, can be overridden)
# ============================================================================

# Project root directory
if [[ -z "$PROJECT_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
fi

# Build directories
BUILD_DIR="${PROJECT_ROOT}/build-release"
DIST_DIR="${PROJECT_ROOT}/dist"
TEMP_DIR="${TMPDIR:-/tmp}/fujisan-build-$$"

# libatari800 is now built automatically by CMake ExternalProject
# No need for ATARI800_SRC_PATH environment variable

# Output file names
APP_NAME="${PROJECT_NAME}.app"
DMG_NAME="${PROJECT_NAME}-v${PROJECT_VERSION}-macOS.dmg"
ARCHIVE_NAME="${PROJECT_NAME}-v${PROJECT_VERSION}-macOS.zip"

# ============================================================================
# QT5 CONFIGURATION (Auto-detection)
# ============================================================================

# Qt5 paths in order of preference
QT5_SEARCH_PATHS=(
    "$CMAKE_PREFIX_PATH"
    "/opt/homebrew/opt/qt@5"
    "/usr/local/opt/qt@5"
    "/opt/local/libexec/qt5"
    "/usr/local/Cellar/qt@5"
    "/Applications/Qt/5.15.2/clang_64"
    "/Users/$(whoami)/Qt/5.15.2/clang_64"
)

# Auto-detect Qt5 installation
detect_qt5_path() {
    for qt_path in "${QT5_SEARCH_PATHS[@]}"; do
        if [[ -n "$qt_path" && -d "$qt_path" && -f "$qt_path/bin/qmake" ]]; then
            QT5_PATH="$qt_path"
            QT5_BIN_PATH="$qt_path/bin"
            MACDEPLOYQT="$qt_path/bin/macdeployqt"
            echo "Found Qt5 at: $QT5_PATH"
            return 0
        fi
    done
    
    echo "ERROR: Qt5 installation not found. Please set CMAKE_PREFIX_PATH or install Qt5."
    echo "Searched paths:"
    printf '  %s\n' "${QT5_SEARCH_PATHS[@]}"
    return 1
}

# ============================================================================
# CODE SIGNING CONFIGURATION
# ============================================================================

# Developer identity (must be set by user or environment)
if [[ -z "$DEVELOPER_ID" ]]; then
    # Try to auto-detect from keychain
    DEVELOPER_ID=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed -E 's/.*"([^"]+)".*/\1/')
    if [[ -z "$DEVELOPER_ID" ]]; then
        echo "WARNING: DEVELOPER_ID not set and no Developer ID Application certificate found."
        echo "Please set DEVELOPER_ID environment variable or ensure certificate is in keychain."
    else
        echo "Auto-detected Developer ID: $DEVELOPER_ID"
    fi
fi

# Team ID (extracted from certificate or environment)
if [[ -z "$DEVELOPER_TEAM_ID" && -n "$DEVELOPER_ID" ]]; then
    DEVELOPER_TEAM_ID=$(echo "$DEVELOPER_ID" | sed -E 's/.*\(([A-Z0-9]+)\).*/\1/')
    echo "Extracted Team ID: $DEVELOPER_TEAM_ID"
fi

# Notarization settings
NOTARIZATION_PROFILE="${NOTARIZATION_PROFILE:-fujisan-notarization}"
NOTARIZATION_TIMEOUT="${NOTARIZATION_TIMEOUT:-1800}"  # 30 minutes

# Code signing options
CODESIGN_OPTIONS="--timestamp --options runtime --force --deep"
ENTITLEMENTS_FILE="${PROJECT_ROOT}/scripts/entitlements.plist"

# ============================================================================
# BUILD CONFIGURATION
# ============================================================================

# CMake build type
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

# macOS deployment target
MACOS_DEPLOYMENT_TARGET="${MACOS_DEPLOYMENT_TARGET:-11.0}"

# CMake generator
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"

# Parallel build jobs
BUILD_JOBS="${BUILD_JOBS:-$(sysctl -n hw.ncpu)}"

# ============================================================================
# DMG CONFIGURATION
# ============================================================================

# DMG settings
DMG_TITLE="Fujisan v${PROJECT_VERSION}"
DMG_BACKGROUND="${PROJECT_ROOT}/images/dmg-background.png"
DMG_WINDOW_SIZE="540,380"
DMG_ICON_SIZE="72"

# DMG layout positions (x,y coordinates)
# Format: x,y where (0,0) is top-left, window is 540x380
DMG_APP_POSITION="140,250"        # App icon - left side, lower
DMG_APPLICATIONS_POSITION="380,250"  # Applications - right side, same height

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

echo_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Cleanup function
cleanup_build() {
    if [[ -n "$TEMP_DIR" && -d "$TEMP_DIR" ]]; then
        echo_info "Cleaning up temporary directory: $TEMP_DIR"
        rm -rf "$TEMP_DIR"
    fi
}

# Validation function
validate_environment() {
    echo_step "Validating build environment..."
    
    local errors=0
    
    # Check for required tools
    local required_tools=("cmake" "make" "git" "codesign" "xcrun")
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo_error "Required tool not found: $tool"
            ((errors++))
        fi
    done
    
    # Check Qt5
    if ! detect_qt5_path; then
        ((errors++))
    fi
    
    # Check Xcode command line tools
    if ! xcode-select -p >/dev/null 2>&1; then
        echo_error "Xcode command line tools not installed. Run: xcode-select --install"
        ((errors++))
    fi
    
    # Check for code signing certificate
    if [[ -z "$DEVELOPER_ID" ]]; then
        echo_warn "No code signing identity found. Unsigned build will be created."
    fi
    
    return $errors
}

# Export configuration for use by other scripts
export PROJECT_NAME PROJECT_VERSION BUNDLE_ID APP_CATEGORY
export PROJECT_ROOT BUILD_DIR DIST_DIR TEMP_DIR
export APP_NAME DMG_NAME ARCHIVE_NAME
export QT5_PATH QT5_BIN_PATH MACDEPLOYQT
export DEVELOPER_ID DEVELOPER_TEAM_ID NOTARIZATION_PROFILE NOTARIZATION_TIMEOUT
export CODESIGN_OPTIONS ENTITLEMENTS_FILE
export CMAKE_BUILD_TYPE MACOS_DEPLOYMENT_TARGET CMAKE_GENERATOR BUILD_JOBS
export DMG_TITLE DMG_BACKGROUND DMG_WINDOW_SIZE DMG_ICON_SIZE
export DMG_APP_POSITION DMG_APPLICATIONS_POSITION