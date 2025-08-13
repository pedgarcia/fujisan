# macOS Build Instructions for Fujisan

This document provides comprehensive instructions for building, signing, and packaging Fujisan for macOS distribution.

## Quick Start

For users who want to build a self-contained app bundle (no external dependencies required):

```bash
# Optional: Set code signing identity
export DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"

# Run the build (libatari800 will be downloaded and built automatically)
./scripts/build-macos-release.sh
```

## Prerequisites

### Required Tools

- **Xcode Command Line Tools**: `xcode-select --install`
- **CMake 3.16+**: `brew install cmake`
- **Qt5**: `brew install qt@5`
- **Git**: Usually pre-installed on macOS

### Apple Developer Requirements (for signing/notarization)

- **Apple Developer Account** with Developer ID certificates
- **Developer ID Application** certificate in Keychain
- **App-specific password** for notarization
- **Notarization profile** configured with `xcrun notarytool`

## Environment Variables

### No Required Environment Variables

The build system now automatically downloads and builds libatari800 as part of the process. No external dependencies or environment variables are required.

### Optional (auto-detected if not set)

- `CMAKE_PREFIX_PATH`: Path to Qt5 installation
  ```bash
  export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5"
  ```

- `DEVELOPER_ID`: Apple Developer ID for code signing
  ```bash
  export DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"
  ```

- `FUJISAN_VERSION`: Override version (default: from git tags)
  ```bash
  export FUJISAN_VERSION="1.0.0"
  ```

## Build Process Overview

The build process consists of these steps:

1. **Environment Validation** - Check tools and certificates
2. **Build libatari800** - Apply patches and compile library  
3. **Build Fujisan** - Compile Qt5 application as app bundle
4. **Deploy Qt5** - Bundle frameworks with macdeployqt
5. **Code Signing** - Sign all components with Developer ID
6. **Notarization** - Submit to Apple and wait for approval
7. **DMG Creation** - Create professional installer DMG

## Script Usage

### Master Build Script

```bash
./scripts/build-macos-release.sh [options]
```

**Options:**
- `--skip-build`: Skip building, use existing build
- `--skip-sign`: Skip code signing (for testing)
- `--skip-notarize`: Skip notarization
- `--skip-dmg`: Skip DMG creation
- `--clean`: Clean build directories first
- `--help`: Show usage information

**Examples:**

```bash
# Full signed and notarized build
./scripts/build-macos-release.sh

# Development build (no signing)
./scripts/build-macos-release.sh --skip-sign --skip-notarize

# Clean build
./scripts/build-macos-release.sh --clean

# Just create DMG from existing build
./scripts/build-macos-release.sh --skip-build
```

### Individual Scripts

You can also run individual build steps:

```bash
# Just create the app icon
./scripts/create-icns.sh

# Just build libatari800
./scripts/build-steps/build-libatari800.sh

# Just sign existing app
./scripts/build-steps/sign-app.sh

# Just notarize existing signed app
./scripts/build-steps/notarize-app.sh

# Just create DMG from existing app
./scripts/build-steps/create-dmg.sh
```

## Code Signing Setup

### 1. Install Developer ID Certificate

1. Download certificate from Apple Developer portal
2. Double-click to install in Keychain
3. Verify installation:
   ```bash
   security find-identity -v -p codesigning
   ```

### 2. Set Up Notarization

1. Create app-specific password in Apple ID account
2. Store credentials for notarization:
   ```bash
   xcrun notarytool store-credentials "fujisan-notarization" \
     --apple-id "your-apple-id@example.com" \
     --team-id "YOUR_TEAM_ID" \
     --password "your-app-specific-password"
   ```

### 3. Environment Variables

```bash
# Auto-detected from certificate name
export DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"

# Or set manually
export DEVELOPER_TEAM_ID="YOUR_TEAM_ID"
export NOTARIZATION_PROFILE="fujisan-notarization"
```

## Build Output

### Directory Structure

```
fujisan/
├── build-release/           # CMake build directory
│   └── Fujisan.app         # Built and signed app bundle
├── dist/                   # Distribution files
│   ├── Fujisan-v1.0.0-macOS.dmg
│   └── Fujisan-v1.0.0-macOS.dmg.sha256
└── scripts/                # Build scripts
```

### App Bundle Structure

```
Fujisan.app/
├── Contents/
│   ├── Info.plist          # App metadata
│   ├── MacOS/
│   │   └── Fujisan         # Main executable
│   ├── Resources/
│   │   └── Fujisan.icns    # App icon
│   ├── Frameworks/         # Qt5 frameworks
│   │   ├── QtCore.framework
│   │   ├── QtGui.framework
│   │   ├── QtWidgets.framework
│   │   ├── QtMultimedia.framework
│   │   └── QtNetwork.framework
│   └── PlugIns/            # Qt5 plugins
│       ├── platforms/
│       ├── imageformats/
│       └── audio/
```

## Troubleshooting

### Common Issues

#### Qt5 Not Found
```
Could NOT find Qt5 (missing: Qt5_DIR)
```
**Solution:** Install Qt5 or set CMAKE_PREFIX_PATH:
```bash
brew install qt@5
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5"
```

#### Code Signing Failed
```
errSecInternalComponent
```
**Solution:** Ensure certificate is in login keychain and Xcode is installed:
```bash
security list-keychains
xcode-select --install
```

#### Notarization Failed
```
The app sandbox is not enabled
```
**Solution:** Check entitlements file and ensure hardened runtime is enabled.

#### DMG Creation Failed
```
hdiutil: create failed - Operation not permitted
```
**Solution:** Grant Terminal full disk access in System Preferences > Security & Privacy.

### Verification Commands

```bash
# Check code signature
codesign --verify --deep --strict --verbose=2 build-release/Fujisan.app

# Check notarization
spctl --assess --type execute --verbose build-release/Fujisan.app

# Check DMG signature  
codesign --verify --verbose dist/Fujisan-v1.0.0-macOS.dmg

# Test DMG integrity
hdiutil verify dist/Fujisan-v1.0.0-macOS.dmg
```

### Debug Mode

For detailed output during build:

```bash
# Enable debug output
export DEBUG=1
./scripts/build-macos-release.sh
```

## Configuration Files

### macos-config.sh
Central configuration file with all build settings. Modify this file to change:
- Bundle identifier
- DMG layout and appearance  
- Code signing options
- Build paths and options

### entitlements.plist
Code signing entitlements for hardened runtime. Modify for different app permissions.

### Info.plist.in
App bundle metadata template. CMake substitutes variables during build.

## Integration with CI/CD

The scripts are designed to work in automated environments:

```bash
# GitHub Actions / CI example
export DEVELOPER_ID="${{ secrets.DEVELOPER_ID }}"
./scripts/build-macos-release.sh --skip-notarize
```

## Security Considerations

- Certificates and credentials are never stored in repository
- Notarization profile stores credentials securely in Keychain
- Scripts validate code signatures before proceeding
- All builds use hardened runtime for security

## Support

For build issues:

1. Check environment with `./scripts/macos-config.sh`
2. Review Apple Developer documentation for code signing
3. Verify all prerequisites are installed
4. Check Apple Developer portal for certificate status

## Version History

- **v1.0.0**: Initial macOS build system
  - Complete app bundle creation
  - Code signing and notarization
  - Professional DMG packaging
  - Comprehensive documentation