#!/bin/bash
#
# test_artifacts.sh - Validate Fujisan build artifacts
#
# Usage: ./tests/test_artifacts.sh [platform]
#
# Platforms: macos, linux, windows, all (default: auto-detect)
#
# Run after ./build.sh to verify all expected files exist,
# have correct structure, and contain required components.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="${PROJECT_ROOT}/dist"

PASS=0
FAIL=0
SKIP=0

red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[1;33m%s\033[0m\n' "$*"; }

pass() { ((PASS++)); green "  PASS: $1"; }
fail() { ((FAIL++)); red   "  FAIL: $1"; }
skip() { ((SKIP++)); yellow "  SKIP: $1"; }

check_file_exists() {
    local pattern="$1"
    local desc="$2"
    local matches
    matches=$(ls $pattern 2>/dev/null | head -1) || true
    if [[ -n "$matches" ]]; then
        pass "$desc ($matches)" >&2
        echo "$matches"
    else
        fail "$desc (pattern: $pattern)" >&2
        echo ""
    fi
}

check_file_min_size() {
    local file="$1"
    local min_bytes="$2"
    local desc="$3"
    if [[ -z "$file" || ! -f "$file" ]]; then
        fail "$desc (file not found)"
        return
    fi
    local size
    size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null || echo 0)
    if (( size >= min_bytes )); then
        pass "$desc (${size} bytes)"
    else
        fail "$desc (${size} bytes < ${min_bytes} required)"
    fi
}

# =====================================================================
# macOS Artifact Validation
# =====================================================================
validate_macos() {
    echo ""
    echo "=== macOS Artifact Validation ==="

    local macos_dir="${DIST_DIR}/macos"
    if [[ ! -d "$macos_dir" ]]; then
        skip "macOS dist directory not found"
        return
    fi

    # Check DMG files exist
    local arm64_dmg
    arm64_dmg=$(check_file_exists "${macos_dir}/Fujisan-*-arm64.dmg" "ARM64 DMG exists")
    local x86_dmg
    x86_dmg=$(check_file_exists "${macos_dir}/Fujisan-*-x86_64.dmg" "x86_64 DMG exists")

    # Size checks (DMGs should be > 10MB)
    if [[ -n "$arm64_dmg" ]]; then
        check_file_min_size "$arm64_dmg" 10000000 "ARM64 DMG size > 10MB"
    fi
    if [[ -n "$x86_dmg" ]]; then
        check_file_min_size "$x86_dmg" 10000000 "x86_64 DMG size > 10MB"
    fi

    # Mount and inspect one DMG if available
    local dmg_to_check="${arm64_dmg:-$x86_dmg}"
    if [[ -n "$dmg_to_check" && "$(uname)" == "Darwin" ]]; then
        local mount_point
        mount_point=$(mktemp -d)
        if hdiutil attach "$dmg_to_check" -mountpoint "$mount_point" -nobrowse -quiet 2>/dev/null; then
            # Check app bundle structure
            local app_dir="${mount_point}/Fujisan.app"
            if [[ -d "$app_dir" ]]; then
                pass "Fujisan.app bundle exists in DMG"

                # Check executable
                if [[ -x "${app_dir}/Contents/MacOS/Fujisan" ]]; then
                    pass "Fujisan executable is present and executable"
                else
                    fail "Fujisan executable missing or not executable"
                fi

                # Check frameworks
                if [[ -d "${app_dir}/Contents/Frameworks" ]]; then
                    local fw_count
                    fw_count=$(ls -d "${app_dir}/Contents/Frameworks/"*.framework 2>/dev/null | wc -l)
                    if (( fw_count > 0 )); then
                        pass "Qt frameworks bundled (${fw_count} found)"
                    else
                        fail "No Qt frameworks found in Frameworks/"
                    fi
                else
                    fail "Contents/Frameworks/ directory missing"
                fi

                # Check FujiNet-PC bundle
                local fn_dir="${app_dir}/Contents/Resources/fujinet"
                if [[ -d "$fn_dir" ]]; then
                    pass "FujiNet-PC resource directory exists"
                    if [[ -f "${fn_dir}/fujinet" || -f "${fn_dir}/macos-arm64/fujinet" || -f "${fn_dir}/macos-x86_64/fujinet" ]]; then
                        pass "FujiNet-PC binary found"
                    else
                        # Check subdirectories
                        local fn_binary
                        fn_binary=$(find "$fn_dir" -name "fujinet" -type f 2>/dev/null | head -1)
                        if [[ -n "$fn_binary" ]]; then
                            pass "FujiNet-PC binary found at ${fn_binary}"
                        else
                            fail "FujiNet-PC binary not found in resources"
                        fi
                    fi
                else
                    skip "FujiNet-PC resources not bundled (may be optional)"
                fi

                # Check Info.plist
                if [[ -f "${app_dir}/Contents/Info.plist" ]]; then
                    pass "Info.plist exists"
                    if grep -q "com.8bitrelics.fujisan" "${app_dir}/Contents/Info.plist" 2>/dev/null; then
                        pass "Bundle ID is correct"
                    else
                        fail "Bundle ID mismatch in Info.plist"
                    fi
                else
                    fail "Info.plist missing"
                fi

                # Check otool linkage
                if command -v otool &>/dev/null; then
                    local missing_libs
                    missing_libs=$(otool -L "${app_dir}/Contents/MacOS/Fujisan" 2>/dev/null | grep "not found" || true)
                    if [[ -z "$missing_libs" ]]; then
                        pass "No missing dynamic libraries (otool)"
                    else
                        fail "Missing libraries: ${missing_libs}"
                    fi
                fi

                # Check codesign (optional)
                if command -v codesign &>/dev/null; then
                    if codesign -v "${app_dir}" 2>/dev/null; then
                        pass "Code signature valid"
                    else
                        skip "Code signature not valid (unsigned build)"
                    fi
                fi
            else
                fail "Fujisan.app not found in mounted DMG"
            fi

            hdiutil detach "$mount_point" -quiet 2>/dev/null || true
        else
            skip "Could not mount DMG for inspection"
        fi
        rmdir "$mount_point" 2>/dev/null || true
    fi

    # Check SHA256 files
    check_file_exists "${macos_dir}/*.sha256" "SHA256 checksum file exists" >/dev/null || true
}

# =====================================================================
# Linux Artifact Validation
# =====================================================================
validate_linux() {
    echo ""
    echo "=== Linux Artifact Validation ==="

    local linux_dir="${DIST_DIR}/linux"
    if [[ ! -d "$linux_dir" ]]; then
        skip "Linux dist directory not found"
        return
    fi

    # Check .deb package (amd64)
    local deb_file
    deb_file=$(check_file_exists "${linux_dir}/fujisan_*_amd64.deb" "amd64 .deb package exists")

    if [[ -n "$deb_file" ]] && command -v dpkg-deb &>/dev/null; then
        # Verify package metadata
        local pkg_name
        pkg_name=$(dpkg-deb -f "$deb_file" Package 2>/dev/null || echo "")
        if [[ "$pkg_name" == "fujisan" ]]; then
            pass "Package name is 'fujisan'"
        else
            fail "Package name is '${pkg_name}', expected 'fujisan'"
        fi

        local pkg_arch
        pkg_arch=$(dpkg-deb -f "$deb_file" Architecture 2>/dev/null || echo "")
        if [[ "$pkg_arch" == "amd64" ]]; then
            pass "Architecture is amd64"
        else
            fail "Architecture is '${pkg_arch}', expected 'amd64'"
        fi

        # Check package contains the binary
        if dpkg-deb -c "$deb_file" 2>/dev/null | grep -q "usr/bin/Fujisan"; then
            pass ".deb contains /usr/bin/Fujisan"
        else
            # Try lowercase
            if dpkg-deb -c "$deb_file" 2>/dev/null | grep -qi "usr/bin/fujisan"; then
                pass ".deb contains fujisan binary"
            else
                fail ".deb does not contain Fujisan binary in /usr/bin/"
            fi
        fi
    elif [[ -n "$deb_file" ]]; then
        skip "dpkg-deb not available for .deb inspection"
    fi

    # Check tarball
    local tarball
    tarball=$(check_file_exists "${linux_dir}/fujisan-*-linux-x64.tar.gz" "x64 tarball exists")

    if [[ -n "$tarball" ]]; then
        check_file_min_size "$tarball" 1000000 "Tarball size > 1MB"

        # Extract and verify
        local extract_dir
        extract_dir=$(mktemp -d)
        if tar xzf "$tarball" -C "$extract_dir" 2>/dev/null; then
            local binary
            binary=$(find "$extract_dir" -name "Fujisan" -type f 2>/dev/null | head -1)
            if [[ -n "$binary" ]]; then
                pass "Fujisan binary found in tarball"
                if [[ -x "$binary" ]]; then
                    pass "Binary is executable"
                else
                    fail "Binary is not executable"
                fi

                # Check dynamic linkage on Linux
                if command -v ldd &>/dev/null && [[ "$(uname)" == "Linux" ]]; then
                    local missing
                    missing=$(ldd "$binary" 2>/dev/null | grep "not found" || true)
                    if [[ -z "$missing" ]]; then
                        pass "No missing shared libraries (ldd)"
                    else
                        fail "Missing libraries: ${missing}"
                    fi
                fi
            else
                fail "Fujisan binary not found in tarball"
            fi
        fi
        rm -rf "$extract_dir"
    fi

    # Check ARM64 variants
    local arm_deb
    arm_deb=$(check_file_exists "${linux_dir}/fujisan_*_arm64.deb" "arm64 .deb package exists" 2>/dev/null || echo "")
    local arm_tar
    arm_tar=$(check_file_exists "${linux_dir}/fujisan-*-linux-arm64.tar.gz" "arm64 tarball exists" 2>/dev/null || echo "")
}

# =====================================================================
# Windows Artifact Validation
# =====================================================================
validate_windows() {
    echo ""
    echo "=== Windows Artifact Validation ==="

    local win_dir="${DIST_DIR}/windows"
    if [[ ! -d "$win_dir" ]]; then
        skip "Windows dist directory not found"
        return
    fi

    # Check ZIP
    local zip_file
    zip_file=$(check_file_exists "${win_dir}/Fujisan-*-windows.zip" "Windows ZIP exists")

    if [[ -n "$zip_file" ]]; then
        check_file_min_size "$zip_file" 1000000 "ZIP size > 1MB"

        if command -v unzip &>/dev/null; then
            local listing
            listing=$(unzip -l "$zip_file" 2>/dev/null || true)

            # Check for main executable
            if echo "$listing" | grep -qi "Fujisan.exe"; then
                pass "Fujisan.exe found in ZIP"
            else
                fail "Fujisan.exe not found in ZIP"
            fi

            # Check Qt DLLs
            local qt_dlls=("Qt5Core.dll" "Qt5Widgets.dll" "Qt5Gui.dll" "Qt5Multimedia.dll" "Qt5Network.dll")
            for dll in "${qt_dlls[@]}"; do
                if echo "$listing" | grep -qi "$dll"; then
                    pass "$dll found in ZIP"
                else
                    fail "$dll not found in ZIP"
                fi
            done

            # Check platform plugin
            if echo "$listing" | grep -qi "platforms/qwindows.dll"; then
                pass "platforms/qwindows.dll found"
            elif echo "$listing" | grep -qi "qwindows.dll"; then
                pass "qwindows.dll found (may be in different location)"
            else
                fail "qwindows.dll platform plugin not found"
            fi

            # Check FujiNet-PC
            if echo "$listing" | grep -qi "fujinet.exe"; then
                pass "fujinet.exe found in ZIP"
            else
                skip "fujinet.exe not in ZIP (FujiNet may not be bundled for Windows)"
            fi
        else
            skip "unzip not available for ZIP inspection"
        fi
    fi
}

# =====================================================================
# Main
# =====================================================================

echo "========================================"
echo " Fujisan Build Artifact Validation"
echo "========================================"
echo "Dist directory: ${DIST_DIR}"

if [[ ! -d "$DIST_DIR" ]]; then
    red "ERROR: dist/ directory not found at ${DIST_DIR}"
    red "Run ./build.sh first to generate artifacts."
    exit 1
fi

PLATFORM="${1:-all}"

case "$PLATFORM" in
    macos)   validate_macos ;;
    linux)   validate_linux ;;
    windows) validate_windows ;;
    all)
        validate_macos
        validate_linux
        validate_windows
        ;;
    *)
        red "Unknown platform: $PLATFORM"
        echo "Usage: $0 [macos|linux|windows|all]"
        exit 1
        ;;
esac

echo ""
echo "========================================"
echo " Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped"
echo "========================================"

if (( FAIL > 0 )); then
    red "FAILED: ${FAIL} check(s) did not pass."
    exit 1
else
    green "All checks passed."
    exit 0
fi
