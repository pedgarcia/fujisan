# Fujisan Build Guide

Fujisan is a modern Qt5-based frontend for the Atari800 emulator, providing native desktop experience across macOS, Windows, and Linux.

## Quick Start

The easiest way to build Fujisan is using the unified build script:

```bash
# Build for specific platform
./build.sh macos          # Both Intel and ARM64 DMGs (ad-hoc signed)
./build.sh macos --sign   # Both Intel and ARM64 DMGs (Developer ID signed)  
./build.sh windows        # Windows ZIP package
./build.sh linux          # Linux DEB and tarball
./build.sh all --sign     # All platforms with macOS signing

# Options
./build.sh macos --clean           # Clean build
./build.sh all --version v1.2.0    # Specify version
```

All outputs go to `dist/`:
- `dist/Fujisan-{version}-arm64.dmg` - macOS Apple Silicon
- `dist/Fujisan-{version}-x86_64.dmg` - macOS Intel
- `dist/Fujisan-{version}-windows.zip` - Windows package
- `dist/fujisan-{version}-linux-x64.tar.gz` - Linux tarball
- `dist/fujisan_{version}_amd64.deb` - Debian/Ubuntu package

## Prerequisites

### All Platforms
- Git
- CMake 3.16 or higher
- C++17 compatible compiler

### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Qt5 for your architecture
# Apple Silicon (ARM64):
brew install qt@5

# Intel (x86_64) - requires Rosetta 2:
arch -x86_64 /usr/local/bin/brew install qt@5
```

### Windows (Cross-compilation from macOS/Linux)
```bash
# Install container runtime
brew install podman    # macOS
sudo apt install podman # Linux
```

### Linux
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install qtbase5-dev qtmultimedia5-dev build-essential
sudo apt install cmake git autoconf automake libtool

# Fedora/RHEL
sudo dnf install qt5-qtbase-devel qt5-qtmultimedia-devel
sudo dnf install gcc gcc-c++ cmake git autotools
```

## Platform-Specific Builds

### macOS Native Build
```bash
# Quick development build for current architecture
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" ..  # ARM64
# OR
cmake -DCMAKE_PREFIX_PATH="/usr/local/opt/qt@5" ..     # Intel
make -j$(sysctl -n hw.ncpu)

# Run directly
./Fujisan.app/Contents/MacOS/Fujisan
```

### Windows Cross-Compilation
```bash
# Uses container with MinGW toolchain
./build.sh windows

# Or manually:
./scripts/build-windows-simple.sh
```

### Linux Build
```bash
# Native build
mkdir build-linux && cd build-linux
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./Fujisan

# Or use container build
./build.sh linux
```

## Build Documentation

For detailed information, see:

- **[Build Checklist](docs/build/CHECKLIST.md)** - Critical verification steps for every build
- **[Platform Guide](docs/build/PLATFORMS.md)** - Detailed platform-specific instructions
- **[Windows Technical Details](docs/build/WINDOWS.md)** - Windows build troubleshooting
- **[Code Signing Reference](docs_local/SIGNING_REFERENCE.md)** - Quick reference for macOS signing & notarization

## Common Issues

### CMakeCache Conflicts
```bash
# Clean CMake cache files before building
rm -rf CMakeCache.txt CMakeFiles
# Or use the --clean flag
./build.sh [platform] --clean
```

### Qt5 Not Found
- macOS: Ensure Qt5 is installed via Homebrew
- Linux: Install qt5-default or qtbase5-dev
- Set CMAKE_PREFIX_PATH if needed

### Windows Build Slow at MOC Step
The MOC (Meta-Object Compiler) step can take 2-5 minutes in containers due to processing 18 header files with Qt meta-objects.

## Development Workflow

### Quick Iteration
```bash
# Development build (fast, ad-hoc signed)
./build.sh macos

# Test changes
open dist/Fujisan-*-arm64.dmg
```

### Release Process
```bash
# 1. Update version in CMakeLists.txt
# 2. Build with signing
./build.sh macos --sign --version v1.2.0

# 3. Notarize (macOS)
./scripts/sign-and-notarize-dmgs.sh --skip-signing

# 4. Create release notes
# 5. Upload to GitHub releases
```

## CI/CD Integration

All build scripts support automation:
- No interactive prompts
- Environment variable configuration
- Exit codes for success/failure
- Comprehensive logging

Example GitHub Actions:
```yaml
- name: Build All Platforms
  run: ./build.sh all --sign
```

## Support

- Check prerequisites are installed
- Review [platform-specific documentation](docs/build/PLATFORMS.md)
- Consult the [build checklist](docs/build/CHECKLIST.md) for verification steps
- See [Windows technical details](docs/build/WINDOWS.md) for Windows-specific issues