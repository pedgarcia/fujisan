# Multi-Platform Build Guide for Fujisan

This comprehensive guide covers building Fujisan on all supported platforms: macOS, Linux, and Windows.

## Overview

Fujisan supports three build environments:
- **macOS Native** - Universal binaries (Intel + Apple Silicon)
- **Linux Native** - Linux containers for cross-compilation
- **Windows Cross-compilation** - MinGW-w64 via containers

## Platform-Specific Quick Start

### macOS (Recommended)
```bash
# Install dependencies
brew install cmake qt@5

# Development build (ad-hoc signing)
./build.sh macos

# Distribution build (Developer ID signing)
./build.sh macos --sign

# Clean build
./build.sh macos --clean --sign
```

### Linux
```bash
# Using Docker/Podman for development
podman run --rm -v "$(pwd):/work" -w /work ubuntu:22.04 bash -c "
apt update && apt install -y build-essential cmake qt5-default git autotools-dev
mkdir -p build-linux && cd build-linux
cmake .. && make -j4
"
```

### Windows (Cross-compilation)
```bash
# Using MinGW container
mkdir -p build-cross-windows && cd build-cross-windows
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    cmake -DCMAKE_SYSTEM_NAME=Windows \
          -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
          -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ..
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw make -j4
```

---

## macOS Native Build

### Prerequisites

**Essential Tools:**
```bash
# Xcode Command Line Tools
xcode-select --install

# Package manager dependencies
brew install cmake qt@5 git autoconf automake
```

**For Distribution (Optional):**
- Apple Developer Account with Developer ID certificates
- Code signing certificates installed in Keychain
- Notarization profile configured

### Build Process

#### 1. Quick Development Build
```bash
# Simple build for development/testing
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5"
make -j4

# Run directly
./Fujisan.app/Contents/MacOS/Fujisan
```

#### 2. Universal Binary (Intel + Apple Silicon)
```bash
# Build for both architectures
./scripts/build-universal-macos.sh

# Output: build-universal/Fujisan.app (universal binary)
```

#### 3. Full Release Build (Signed + Notarized + DMG)
```bash
# Set optional code signing identity
export DEVELOPER_ID="Developer ID Application: Your Name (TEAMID)"

# Complete release build
./scripts/build-macos-release.sh

# Output: dist/Fujisan-v1.0.0-macOS.dmg
```

### macOS Build Scripts

| Script | Purpose | Output |
|--------|---------|---------|
| `build-macos-release.sh` | Complete signed release | `.dmg` file |
| `build-universal-macos.sh` | Universal binary | `.app` bundle |
| `build-steps/build-libatari800.sh` | libatari800 only | Static library |
| `build-steps/sign-app.sh` | Code signing only | Signed `.app` |
| `build-steps/notarize-app.sh` | Notarization only | Notarized `.app` |
| `build-steps/create-dmg.sh` | DMG packaging | `.dmg` installer |

### macOS Output Structure

```
fujisan/
├── build-release/           # Standard build
│   └── Fujisan.app         # Single-architecture app
├── build-universal/        # Universal build  
│   └── Fujisan.app         # Multi-architecture app
├── dist/                   # Distribution files
│   ├── Fujisan-v1.0.0-macOS.dmg
│   └── Fujisan-v1.0.0-macOS.dmg.sha256
```

**App Bundle Structure:**
```
Fujisan.app/
├── Contents/
│   ├── Info.plist          # App metadata
│   ├── MacOS/Fujisan       # Executable (universal)
│   ├── Resources/Fujisan.icns
│   ├── Frameworks/         # Qt5 frameworks (bundled)
│   └── PlugIns/           # Qt5 plugins
```

### macOS Configuration

**Environment Variables:**
- `CMAKE_PREFIX_PATH`: Qt5 location (auto-detected: `/opt/homebrew/opt/qt@5`)
- `DEVELOPER_ID`: Code signing identity (optional)
- `TARGET_ARCH`: `arm64`, `x86_64`, or `universal`

**Homebrew Paths:**
- **Apple Silicon**: `/opt/homebrew/opt/qt@5`
- **Intel**: `/usr/local/opt/qt@5`

---

## Linux Native Build

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git autotools-dev libtool pkg-config
sudo apt install qt5-default qtbase5-dev qtmultimedia5-dev qttools5-dev-tools
```

**RHEL/CentOS/Fedora:**
```bash
sudo dnf install gcc gcc-c++ cmake git autotools libtool pkgconfig
sudo dnf install qt5-qtbase-devel qt5-qtmultimedia-devel qt5-qttools-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake git autotools qt5-base qt5-multimedia qt5-tools
```

### Build Process

#### 1. Standard Linux Build
```bash
# Configure and build
mkdir -p build-linux && cd build-linux
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run
./Fujisan
```

#### 2. Container-Based Build (Recommended)
```bash
# Ubuntu 22.04 container
podman run --rm -v "$(pwd):/work" -w /work ubuntu:22.04 bash -c "
apt update && apt install -y build-essential cmake git autotools-dev libtool
apt install -y qtbase5-dev qtmultimedia5-dev qttools5-dev-tools
mkdir -p build-container && cd build-container
cmake .. && make -j4
"
```

#### 3. AppImage Build (Future)
```bash
# Using linuxdeploy for portable AppImage
# TODO: Add AppImage build script
```

### Linux Build Configuration

**CMake Options:**
```cmake
# Standard Linux configuration
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
```

**Dependencies:**
- Qt5 Core, GUI, Widgets, Multimedia, Network
- autotools (autoconf, automake, libtool)
- Standard build tools (gcc, make, pkg-config)

### Linux Output Structure

```
build-linux/
├── Fujisan                 # Main executable
├── atari800-src/          # Built atari800 source
│   └── src/libatari800.a  # Static library
```

**Installation:**
```bash
sudo make install          # Install to CMAKE_INSTALL_PREFIX
# OR
./Fujisan                  # Run from build directory
```

---

## Windows Cross-Compilation

### Prerequisites

**Host System Requirements:**
- macOS or Linux host
- Podman or Docker installed
- Git for source control

**Container Image:**
- `maxrd2/arch-mingw` - Arch Linux with MinGW-w64 toolchain
- Provides Qt5, CMake, autotools, and cross-compilation tools

### Build Process

#### 1. Basic Windows Build
```bash
# Create build directory
mkdir -p build-cross-windows && cd build-cross-windows

# Configure with cross-compilation
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    cmake \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    ..

# Build Fujisan
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    make -j4

# Output: Fujisan.exe (~5MB)
```

#### 2. Windows Build with FujiNet Support
```bash
# Enable FujiNet/NetSIO (experimental)
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    cmake -DENABLE_FUJINET=ON \
          -DCMAKE_SYSTEM_NAME=Windows \
          -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
          -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
          ..
```

#### 3. Create Deployment Package
```bash
# Extract all required DLLs and assets
./scripts/create-windows-package.sh build-cross-windows/Fujisan.exe
```

### Windows Build Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| `build-libatari800-simple.sh` | Basic libatari800 build | Default Windows build |
| `build-libatari800-fujinet.sh` | libatari800 with NetSIO | FujiNet-enabled build |
| `create-minimal-makefile.sh` | Fallback build system | When autotools fails |

### Windows Dependencies

**Runtime DLLs (21 total):**

**Qt5 Libraries (5):**
- Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll
- Qt5Network.dll, Qt5Multimedia.dll

**MinGW Runtime (3):**
- libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll

**System Libraries (13):**
- libiconv-2.dll, libpcre2-16-0.dll, libssp-0.dll
- zlib1.dll, libharfbuzz-0.dll, libpng16-16.dll
- libfreetype-6.dll, libglib-2.0-0.dll, libgraphite2.dll
- libpcre-1.dll, libintl-8.dll, libbz2-1.dll

**Qt Platform Plugin:**
- `platforms/qwindows.dll`

**UI Assets:**
- `images/` directory with PNG graphics

### Windows Package Structure

```
windows-release/
├── Fujisan.exe             # Main executable (4.98MB)
├── *.dll                   # 21 runtime dependencies
├── platforms/
│   └── qwindows.dll        # Qt platform plugin
├── images/                 # UI graphics
│   ├── fujisanlogo.png
│   ├── atari810*.png       # Drive states
│   ├── cassette*.png       # Tape states
│   └── cartridge*.png      # Cartridge states  
└── README.txt              # Deployment instructions
```

### Windows Deployment

**Testing:**
- Tested on Windows 10+ (x64)
- Confirmed working on Windows ARM VM
- All DLLs must be in same directory as Fujisan.exe

**Installation:**
1. Extract complete package to any directory
2. Run `Fujisan.exe` 
3. Configure ROM paths through UI

---

## Build Configuration Reference

### CMake Options (All Platforms)

| Option | Values | Description |
|--------|--------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug`, `Release` | Build optimization |
| `CMAKE_PREFIX_PATH` | Path | Qt5 installation path |
| `ENABLE_FUJINET` | `ON`, `OFF` | FujiNet/NetSIO support |
| `CMAKE_OSX_ARCHITECTURES` | `arm64`, `x86_64` | macOS target arch |
| `CMAKE_SYSTEM_NAME` | `Windows` | Cross-compilation target |

### Environment Variables

| Variable | Platform | Description |
|----------|----------|-------------|
| `CMAKE_PREFIX_PATH` | macOS | Qt5 location |
| `DEVELOPER_ID` | macOS | Code signing identity |
| `ATARI800_SRC_PATH` | All | libatari800 source (optional) |
| `ENABLE_FUJINET` | All | Enable NetSIO support |

### libatari800 Configuration

**Applied Patches:**
1. `0001-libatari800-disk-api.patch` - Fujisan API extensions
2. `0002-windows-ulong-conflicts.patch` - Windows type fixes

**Build Target:**
- `--target=libatari800` produces static library
- FujiNet support via `--enable-netsio --enable-rnetwork`
- Platform-specific configure flags applied automatically

---

## Troubleshooting

### Common Issues

#### Qt5 Not Found
**Symptoms:** `Could NOT find Qt5`
**Solutions:**
- macOS: `brew install qt@5`
- Linux: `apt install qtbase5-dev`
- Set `CMAKE_PREFIX_PATH` manually

#### libatari800 Build Fails
**Symptoms:** `configure: error: No autotools`
**Solutions:**
- Install autotools: `brew install autoconf automake`
- Use minimal build fallback (Windows)
- Check atari800 source download

#### Missing DLLs (Windows)
**Symptoms:** Runtime DLL errors
**Solutions:**
- Copy additional DLLs from container
- Ensure `platforms/qwindows.dll` exists
- Check `windows-release/` package completeness

#### Code Signing Failed (macOS)
**Symptoms:** `errSecInternalComponent`
**Solutions:**
- Install Xcode Command Line Tools
- Verify certificate in Keychain
- Check Developer ID format

### Debug Commands

```bash
# Check Qt5 configuration
qmake --version
find /usr -name "*Qt5*" 2>/dev/null

# Check build dependencies
ldd Fujisan                 # Linux
otool -L Fujisan.app/Contents/MacOS/Fujisan  # macOS

# Verify signatures (macOS)
codesign --verify --deep Fujisan.app
spctl --assess --type execute Fujisan.app
```

### Getting Help

1. Check prerequisite installation
2. Verify environment variables
3. Test with minimal build first
4. Review platform-specific documentation
5. Check container/Docker functionality

---

## Development Workflow

### Recommended Development Setup

**macOS Developers:**
```bash
# Quick development build
./build.sh macos

# Test changes
open dist/macos/Fujisan-*-arm64.dmg  # Mount and test

# Create signed release when ready
./build.sh macos --sign
./scripts/sign-and-notarize-dmgs.sh --skip-signing
```

**Linux Developers:**
```bash
# Container development
podman run -it -v "$(pwd):/work" ubuntu:22.04
# Install dependencies, then build as above
```

**Cross-platform Testing:**
```bash
# Test all platforms
./build.sh macos --sign                 # macOS
./build.sh linux                        # Linux  
./build.sh windows                      # Windows
./build.sh all --sign                   # All platforms
```

### CI/CD Integration

All build scripts support automation:
- No interactive prompts
- Environment variable configuration  
- Exit codes for success/failure
- Comprehensive logging output

Example GitHub Actions:
```yaml
- name: Build macOS
  run: ./build.sh macos --sign
  
- name: Build Windows  
  run: ./build.sh windows
  
- name: Build Linux
  run: ./build.sh linux
  
- name: Build All Platforms
  run: ./build.sh all --sign
```

---

## Platform Comparison

| Feature | macOS | Linux | Windows |
|---------|-------|-------|---------|
| **Native Build** | ✅ | ✅ | ❌ (Cross-compile) |
| **Container Build** | ✅ | ✅ | ✅ |
| **Universal Binary** | ✅ | ❌ | ❌ |
| **Code Signing** | ✅ | ❌ | ❌ |
| **App Bundle** | ✅ | ❌ | ❌ |  
| **FujiNet Support** | ✅ | ✅ | ⚠️ (Limited) |
| **Static Linking** | ❌ | ✅ | ⚠️ (Partial) |
| **Package Manager** | ✅ Homebrew | ✅ apt/dnf | ❌ |

### Best Practices by Platform

**macOS:**
- Use Homebrew for dependencies
- Build universal binaries for distribution
- Sign and notarize for Gatekeeper compatibility
- Use app bundles for proper macOS integration

**Linux:**
- Use system package manager when possible
- Consider AppImage for portable distribution
- Test on multiple distributions
- Static link for maximum compatibility

**Windows:**
- Use container cross-compilation from Linux/macOS
- Include complete DLL package
- Test on target Windows versions
- Consider installer creation for distribution

---

This comprehensive guide provides everything needed to build Fujisan across all supported platforms. Each platform has its own strengths and considerations, but all produce fully functional Atari 800 emulation with the modern Qt5 interface.