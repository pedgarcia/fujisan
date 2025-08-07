# Fujisan Build Instructions

This document provides comprehensive instructions for building Fujisan on all supported platforms.

## Table of Contents
- [Quick Start - Unified Build Script](#quick-start---unified-build-script)
- [Prerequisites](#prerequisites)
- [macOS Builds](#macos-builds)
  - [Native Single-Architecture Build](#native-single-architecture-build)
  - [Universal Binary Build](#universal-binary-build)
  - [Platform-Specific DMGs](#platform-specific-dmgs)
  - [Code Signing](#code-signing)
- [Windows Cross-Compilation](#windows-cross-compilation)
- [Linux Build](#linux-build)
- [Build Scripts Reference](#build-scripts-reference)

## Quick Start - Unified Build Script

The easiest way to build Fujisan for any platform:

```bash
# Build for specific platform
./build.sh macos          # Both Intel and ARM64 DMGs
./build.sh windows        # Windows ZIP package
./build.sh linux          # Linux DEB and tarball
./build.sh all            # All platforms

# Options
./build.sh macos --clean  # Clean build
./build.sh all --version v1.2.0  # Specify version

# All outputs go to dist/
ls dist/
# Fujisan-1.2.0-arm64.dmg
# Fujisan-1.2.0-x86_64.dmg  
# Fujisan-1.2.0-windows.zip
# fujisan-1.2.0-linux-x64.tar.gz
# fujisan_1.2.0_amd64.deb
```

## Prerequisites

### All Platforms
- Git
- CMake 3.16 or higher
- C++17 compatible compiler

### macOS
- Xcode Command Line Tools
- Qt 5.15+ (both architectures for universal builds)
  ```bash
  # For Apple Silicon (ARM64)
  brew install qt@5
  
  # For Intel (x86_64) - requires Rosetta 2
  arch -x86_64 /usr/local/bin/brew install qt@5
  ```

### Windows Cross-Compilation (from macOS/Linux)
- Docker or Podman
  ```bash
  # macOS
  brew install podman  # or docker
  
  # Linux
  sudo apt install podman  # or docker
  ```

### Linux
- Qt5 development packages
  ```bash
  sudo apt install qtbase5-dev qtmultimedia5-dev build-essential autoconf automake
  ```

## macOS Builds

### Native Single-Architecture Build

Build for your current Mac architecture:

```bash
# 1. Clone the repository
git clone https://github.com/atari800/fujisan.git
cd fujisan

# 2. Create build directory
mkdir build && cd build

# 3. Configure with CMake
# For Apple Silicon Macs:
cmake -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" ..

# For Intel Macs:
cmake -DCMAKE_PREFIX_PATH="/usr/local/opt/qt@5" ..

# 4. Build
make -j$(sysctl -n hw.ncpu)

# 5. Deploy Qt frameworks
# For Apple Silicon:
/opt/homebrew/opt/qt@5/bin/macdeployqt Fujisan.app

# For Intel:
/usr/local/opt/qt@5/bin/macdeployqt Fujisan.app

# Result: Fujisan.app in build directory
```

### Universal Binary Build

Build a universal app that runs on both Intel and Apple Silicon (NOT RECOMMENDED - see Platform-Specific DMGs instead):

```bash
# Use the provided script
./scripts/build-universal-macos-complete.sh

# Options:
#   --clean       Clean build directories before starting
#   --skip-arm64  Skip ARM64 build (use existing)
#   --skip-x86_64 Skip x86_64 build (use existing)
#   --sign        Sign the application (requires certificates)

# Output: dist/Fujisan-{version}-universal.dmg
```

**Note:** Universal binaries with Qt are problematic because Qt frameworks from different architectures may not be compatible. Use platform-specific builds instead.

### Platform-Specific DMGs (RECOMMENDED)

Create separate DMGs for Intel and Apple Silicon Macs:

```bash
# Build both architectures and create DMGs
./scripts/build-macos-separate-dmgs.sh

# Options:
#   --clean       Clean build directories before starting
#   --skip-arm64  Skip ARM64 build
#   --skip-x86_64 Skip x86_64 build
#   --skip-dmg    Skip DMG creation

# Output:
#   dist/Fujisan-{version}-arm64.dmg   - For Apple Silicon Macs
#   dist/Fujisan-{version}-x86_64.dmg  - For Intel Macs
```

Each DMG contains:
- `Fujisan.app` - Properly configured for the target architecture
- Applications folder shortcut for easy installation
- README.txt with installation instructions

### Code Signing

To sign the apps with an Apple Developer certificate:

```bash
# 1. Check available signing identities
security find-identity -v -p codesigning

# 2. Sign both apps
./scripts/sign-macos-apps.sh

# This will:
# - Create entitlements file with proper permissions for emulation
# - Sign all frameworks and libraries
# - Sign the main application bundle
# - Verify the signature
```

For distribution, you need:
- **Development Certificate**: For testing on your own Mac
- **Developer ID Application**: For direct distribution (outside App Store)
- **Apple Distribution**: For Mac App Store

## Windows Cross-Compilation

Build Windows executables from macOS or Linux using Docker/Podman:

### Build Process

```bash
# 1. Build Windows executable using MinGW container
./scripts/build-windows-cross.sh

# Note: Script uses Podman by default. For Docker, edit the script 
# and replace 'podman' with 'docker'

# 2. Create Windows release package with all dependencies
./create-windows-release-with-audio.sh

# Output: build-windows/
# Contains Fujisan.exe and all required DLLs including Qt5 and audio plugins
```

### Windows Release Package Structure
```
build-windows/
├── Fujisan.exe           # Main executable
├── Qt5*.dll             # Qt framework libraries
├── lib*.dll             # System libraries
├── platforms/           # Qt platform plugins
│   └── qwindows.dll
├── audio/               # Qt audio plugins
│   └── qtaudio_windows.dll
└── mediaservice/        # Qt media service plugins
    └── qtmedia_audioengine.dll
```

### Troubleshooting Windows Build

If audio doesn't work:
- Ensure audio plugins are included (qtaudio_windows.dll)
- Check that mediaservice plugins are present

If LEDs don't work:
- Verify patches were applied correctly
- Check that disk activity callbacks are enabled

## Linux Build

### Docker/Podman Build (Recommended)

Build Linux packages using the same setup as GitHub Actions:

```bash
# Build .deb and .tar.gz packages
./scripts/build-linux-docker.sh

# Options:
#   --clean          Clean build directories before starting
#   --no-deb         Skip .deb package creation
#   --no-tarball     Skip .tar.gz creation
#   --appimage       Also create AppImage (experimental)
#   --keep-container Keep container image after build
#   --version        Set version (default: from git)

# Output:
#   dist/fujisan_{version}_amd64.deb        - Debian/Ubuntu package
#   dist/fujisan-{version}-linux-x64.tar.gz - Portable tarball
```

This builds using Ubuntu 24.04 LTS in a container, ensuring compatibility with:
- Ubuntu 22.04+ 
- Debian 12+
- Other modern Linux distributions

### Native Build on Linux

For direct compilation on a Linux system:

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install -y \
    build-essential cmake git pkg-config \
    qtbase5-dev qtmultimedia5-dev \
    libgl1-mesa-dev libasound2-dev libpulse-dev \
    autoconf automake

# 2. Clone and build
git clone https://github.com/atari800/fujisan.git
cd fujisan
mkdir build && cd build

# 3. Configure and build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Result: Fujisan executable in build directory
```

### Package Installation

**Debian/Ubuntu (.deb):**
```bash
sudo dpkg -i fujisan_{version}_amd64.deb
# If dependencies are missing:
sudo apt-get install -f
```

**Portable (.tar.gz):**
```bash
tar xzf fujisan-{version}-linux-x64.tar.gz
cd fujisan-portable
./fujisan.sh
```

## Build Scripts Reference

### Core Build Scripts

| Script | Purpose | Platform |
|--------|---------|----------|
| `scripts/build-macos-separate-dmgs.sh` | Create platform-specific DMGs for ARM64 and x86_64 | macOS |
| `scripts/build-universal-macos-complete.sh` | Create universal binary (not recommended) | macOS |
| `scripts/sign-macos-apps.sh` | Sign apps with Apple Developer certificate | macOS |
| `scripts/build-windows-cross.sh` | Cross-compile Windows exe using MinGW | macOS/Linux |
| `create-windows-release-with-audio.sh` | Package Windows release with Qt DLLs and audio plugins | macOS/Linux |
| `scripts/build-linux-docker.sh` | Build Linux .deb and .tar.gz packages using Docker/Podman | Any |

### Helper Scripts

| Script | Purpose |
|--------|---------|
| `scripts/configure-atari800.sh` | Configure libatari800 build |
| `patches/apply-patches.sh` | Apply Fujisan patches to atari800 source |
| `scripts/create-minimal-makefile.sh` | Create Makefile for Windows builds |

### Important Environment Variables

- `ATARI800_SRC_PATH`: Path to atari800 source (auto-downloaded by CMake)
- `CMAKE_PREFIX_PATH`: Path to Qt installation
- `DEVELOPER_ID`: Apple Developer signing identity

## Patch System

Fujisan requires patches to the atari800 source for enhanced functionality:

1. **0001-libatari800-disk-api.patch**: Disk management API
2. **0002-windows-ulong-conflicts.patch**: Windows type conflict fixes (Windows only)
3. **0003-disk-activity-callback-integration.patch**: Disk LED activity monitoring

Patches are automatically applied during the CMake build process.

## Build Organization

### Directory Structure
```
fujisan/
├── build/                  # Native build (single architecture)
├── build-arm64/           # ARM64 build for universal binary
├── build-x86_64/          # x86_64 build for universal binary
├── build-universal/       # Universal binary assembly
├── build-cross-windows/   # Windows cross-compilation temp
├── build-windows/         # Windows release package
└── dist/                  # Distribution files (DMGs, etc.)
```

### Clean Build

To clean all build artifacts:

```bash
# macOS
rm -rf build/ build-arm64/ build-x86_64/ build-universal/ dist/

# Windows cross-compilation
rm -rf build-cross-windows/ build-windows/

# Linux
rm -rf build/
```

## Troubleshooting

### macOS Issues

**"Library not loaded" error:**
- Ensure Qt frameworks are deployed with `macdeployqt`
- Check that the correct architecture Qt is used

**"Bad CPU type" error:**
- You're trying to run x86_64 binary on ARM64 Mac without Rosetta 2
- Or running ARM64 binary on Intel Mac
- Use the appropriate architecture-specific build

**Signature issues:**
- Ad-hoc signatures work for local testing
- For distribution, use proper Developer ID certificate
- Right-click and select "Open" to bypass Gatekeeper warnings

### Windows Issues

**Missing DLL errors:**
- Ensure all Qt DLLs are copied from the container
- Check that Visual C++ redistributables are included
- Use `create-windows-release-with-audio.sh` for complete package

**No audio:**
- Verify audio/ and mediaservice/ directories contain plugins
- Check qtaudio_windows.dll is present

### General Issues

**CMake can't find Qt:**
- Set CMAKE_PREFIX_PATH to Qt installation directory
- Ensure Qt version is 5.15 or higher

**Patches fail to apply:**
- Check you're using compatible atari800 version (commit 575a943b)
- Ensure git is configured with user.email and user.name
- Try manual patch application with `patch -p1`

## Testing Your Build

After building, test the following:

1. **Basic functionality:**
   - Load and run Atari disk images
   - Test keyboard input
   - Verify sound output

2. **Platform-specific features:**
   - Disk activity LEDs
   - Drag and drop support
   - Full-screen mode

3. **Architecture verification:**
   ```bash
   # Check binary architecture
   file Fujisan.app/Contents/MacOS/Fujisan  # macOS
   file Fujisan.exe                          # Windows
   
   # Check Qt framework architecture (macOS)
   file Fujisan.app/Contents/Frameworks/QtCore.framework/QtCore
   ```

## Support

For issues or questions:
- GitHub Issues: https://github.com/atari800/fujisan/issues
- Documentation: https://github.com/atari800/fujisan/wiki