#!/bin/bash

# Make script executable
chmod +x "$0"

# Fujisan DMG Signing and Notarization Script
# This script signs and notarizes the DMG files created by the build process

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$PROJECT_ROOT/dist/macos"

echo $PROJECT_ROOT
echo $DIST_DIR

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUNDLE_ID="com.atari.fujisan"
NOTARIZATION_PROFILE="fujisan-notarization"  # You'll need to create this

# Parse command line options
SKIP_SIGNING=false
SKIP_NOTARIZATION=false
DEVELOPER_ID=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-signing)
            SKIP_SIGNING=true
            shift
            ;;
        --skip-notarization)
            SKIP_NOTARIZATION=true
            shift
            ;;
        --developer-id)
            DEVELOPER_ID="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --skip-signing       Skip code signing step"
            echo "  --skip-notarization  Skip notarization step"
            echo "  --developer-id ID    Specify Developer ID (otherwise auto-detect)"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to find Developer ID
find_developer_id() {
    if [[ -n "$DEVELOPER_ID" ]]; then
        echo "$DEVELOPER_ID"
        return
    fi
    
    log_info "Looking for Developer ID Application certificate..."
    
    # Try to find Developer ID Application certificate
    local dev_id=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/')
    
    if [[ -z "$dev_id" ]]; then
        log_error "No Developer ID Application certificate found"
        log_info "Please create one at https://developer.apple.com/account/resources/certificates/list"
        log_info "Or specify manually with --developer-id option"
        exit 1
    fi
    
    log_success "Found certificate: $dev_id"
    echo "$dev_id"
}

# Function to sign a DMG
sign_dmg() {
    local dmg_path="$1"
    local dev_id="$2"
    
    if [[ ! -f "$dmg_path" ]]; then
        log_error "DMG not found: $dmg_path"
        return 1
    fi
    
    log_info "Signing DMG: $(basename "$dmg_path")"
    
    # Sign the DMG
    codesign --force --verify --verbose --sign "$dev_id" "$dmg_path"
    
    # Verify the signature
    log_info "Verifying signature..."
    codesign --verify --deep --strict --verbose=2 "$dmg_path"
    
    log_success "DMG signed successfully: $(basename "$dmg_path")"
}

# Function to notarize a DMG
notarize_dmg() {
    local dmg_path="$1"
    
    if [[ ! -f "$dmg_path" ]]; then
        log_error "DMG not found: $dmg_path"
        return 1
    fi
    
    log_info "Submitting for notarization: $(basename "$dmg_path")"
    
    # Submit for notarization
    local submission_id=$(xcrun notarytool submit "$dmg_path" \
        --keychain-profile "$NOTARIZATION_PROFILE" \
        --wait --output-format json | jq -r '.id')
    
    if [[ "$submission_id" == "null" || -z "$submission_id" ]]; then
        log_error "Failed to submit for notarization"
        return 1
    fi
    
    log_info "Submission ID: $submission_id"
    
    # Check status
    log_info "Checking notarization status..."
    local status=$(xcrun notarytool info "$submission_id" \
        --keychain-profile "$NOTARIZATION_PROFILE" \
        --output-format json | jq -r '.status')
    
    if [[ "$status" == "Accepted" ]]; then
        log_success "Notarization successful!"
        
        # Staple the notarization
        log_info "Stapling notarization to DMG..."
        xcrun stapler staple "$dmg_path"
        
        # Verify stapling
        log_info "Verifying stapled notarization..."
        xcrun stapler validate "$dmg_path"
        
        log_success "DMG notarized and stapled: $(basename "$dmg_path")"
    else
        log_error "Notarization failed with status: $status"
        
        # Get detailed log
        log_info "Fetching notarization log..."
        xcrun notarytool log "$submission_id" \
            --keychain-profile "$NOTARIZATION_PROFILE"
        
        return 1
    fi
}

# Function to setup notarization profile
setup_notarization_profile() {
    log_info "Checking notarization profile: $NOTARIZATION_PROFILE"
    
    # Check if profile exists
    if ! xcrun notarytool history --keychain-profile "$NOTARIZATION_PROFILE" &>/dev/null; then
        log_warning "Notarization profile '$NOTARIZATION_PROFILE' not found"
        log_info "To create it, run:"
        log_info "xcrun notarytool store-credentials \"$NOTARIZATION_PROFILE\" \\"
        log_info "    --apple-id \"your-apple-id@example.com\" \\"
        log_info "    --team-id \"YOUR_TEAM_ID\" \\"
        log_info "    --password \"app-specific-password\""
        log_info ""
        log_info "Where:"
        log_info "- apple-id: Your Apple ID email"
        log_info "- team-id: Your Developer Team ID (found in Apple Developer portal)"
        log_info "- password: App-specific password (generated in Apple ID settings)"
        
        if [[ "$SKIP_NOTARIZATION" == "false" ]]; then
            exit 1
        fi
    else
        log_success "Notarization profile found"
    fi
}

# Main execution
main() {
    log_info "Starting Fujisan DMG signing and notarization process"
    
    # Check if dist directory exists
    if [[ ! -d "$DIST_DIR" ]]; then
        log_error "Distribution directory not found: $DIST_DIR"
        log_info "Please run the build script first to generate DMGs"
        exit 1
    fi
    
    # Find DMG files
    local dmg_files=($(find "$DIST_DIR" -name "Fujisan-*.dmg" -type f))
    
    if [[ ${#dmg_files[@]} -eq 0 ]]; then
        log_error "No Fujisan DMG files found in $DIST_DIR"
        log_info "Expected files like: Fujisan-v1.0.0-arm64.dmg, Fujisan-v1.0.0-x86_64.dmg"
        exit 1
    fi

    log_info "Found ${#dmg_files[@]} DMG file(s) to process"
    
    # Setup notarization if not skipping
    if [[ "$SKIP_NOTARIZATION" == "false" ]]; then
        setup_notarization_profile
    fi
    
    # Find Developer ID
    local dev_id=""
    if [[ "$SKIP_SIGNING" == "false" ]]; then
        dev_id=$(find_developer_id)
    fi
    # Process each DMG
    for dmg_file in "${dmg_files[@]}"; do
        log_info "Processing: $(basename "$dmg_file")"
        
        # Sign the DMG
        if [[ "$SKIP_SIGNING" == "false" ]]; then
            sign_dmg "$dmg_file" "$dev_id"
        else
            log_warning "Skipping signing for: $(basename "$dmg_file")"
        fi
        
        # Notarize the DMG
        if [[ "$SKIP_NOTARIZATION" == "false" ]]; then
            notarize_dmg "$dmg_file"
        else
            log_warning "Skipping notarization for: $(basename "$dmg_file")"
        fi
        
        log_success "Completed processing: $(basename "$dmg_file")"
    done
    
    log_success "All DMGs processed successfully!"
    
    # Final verification
    log_info "Final verification of all DMGs..."
    for dmg_file in "${dmg_files[@]}"; do
        log_info "Verifying: $(basename "$dmg_file")"
        
        if [[ "$SKIP_SIGNING" == "false" ]]; then
            codesign --verify --deep --strict "$dmg_file"
            log_success "Code signature valid: $(basename "$dmg_file")"
        fi
        
        if [[ "$SKIP_NOTARIZATION" == "false" ]]; then
            xcrun stapler validate "$dmg_file"
            log_success "Notarization valid: $(basename "$dmg_file")"
        fi
    done
    
    log_success "Fujisan DMGs are ready for distribution!"
    log_info "Files location: $DIST_DIR"
    
    # Show what was processed
    log_info "Processed files:"
    for dmg_file in "${dmg_files[@]}"; do
        echo "  - $(basename "$dmg_file")"
    done
}

# Check dependencies
check_dependencies() {
    local missing_deps=()
    
    if ! command -v codesign &> /dev/null; then
        missing_deps+=("codesign (Xcode Command Line Tools)")
    fi
    
    if ! command -v xcrun &> /dev/null; then
        missing_deps+=("xcrun (Xcode Command Line Tools)")
    fi
    
    if [[ "$SKIP_NOTARIZATION" == "false" ]] && ! command -v jq &> /dev/null; then
        missing_deps+=("jq (install with: brew install jq)")
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        log_error "Missing required dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        exit 1
    fi
}

# Run dependency check and main function
check_dependencies
main

log_success "Script completed successfully!"
