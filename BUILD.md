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

**IMPORTANT: The recommended build method uses Docker/Podman for consistency and portability.**

#### For Container-based Builds (Recommended)
```bash
# Ubuntu/Debian
sudo apt install podman
# OR
sudo apt install docker.io
# Add your user to the docker group (if using Docker)
sudo usermod -aG docker $USER
# Then log out and log back in for group changes to take effect

# Fedora/RHEL
sudo dnf install podman
```

#### For Manual/Native Builds (Advanced)
If building without Docker/Podman, install these dependencies:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install qtbase5-dev qtmultimedia5-dev build-essential
sudo apt install cmake git autoconf automake libtool
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
sudo apt install libxrandr-dev libxss-dev libxcursor-dev libxinerama-dev
sudo apt install libxi-dev libxext-dev libxfixes-dev libxrender-dev
sudo apt install libxcb1-dev libx11-xcb-dev libxcb-glx0-dev
sudo apt install libfontconfig1-dev libfreetype6-dev
sudo apt install libasound2-dev libpulse-dev
sudo apt install libsdl2-dev  # Optional: for joystick support

# Fedora/RHEL
sudo dnf install qt5-qtbase-devel qt5-qtmultimedia-devel
sudo dnf install gcc gcc-c++ cmake git autotools
sudo dnf install mesa-libGL-devel mesa-libGLU-devel
sudo dnf install libXrandr-devel libXss-devel libXcursor-devel
sudo dnf install libXinerama-devel libXi-devel libXext-devel
sudo dnf install SDL2-devel  # Optional: for joystick support
```

**Note:** Manual builds may encounter issues depending on your system's library versions. The container-based build provides a consistent Ubuntu 22.04 environment.

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

#### Recommended: Container-based Build
```bash
# Requires Docker or Podman (see Prerequisites section)
./build.sh linux

# Output:
# - dist/linux/fujisan_{version}_amd64.deb
# - dist/linux/fujisan-{version}-linux-x64.tar.gz

# For ARM64 (Raspberry Pi 4/5, etc.)
./build.sh linux-arm64
```

**Why use containers?**
- Consistent Ubuntu 22.04 environment
- All dependencies pre-installed
- Avoids version conflicts with system libraries
- Works the same on any Linux distribution

#### Advanced: Manual/Native Build
```bash
# Only recommended if you can't use Docker/Podman
# Requires all dependencies from Prerequisites section
mkdir build-linux && cd build-linux
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./Fujisan
```

**Note:** Manual builds may fail with version-related CMake errors or missing dependencies. Use the container build for reliable results.

#### Linux CPU Compatibility (x86-64-v2)

Release builds target **x86-64-v2** so they run on Intel Ivy Bridge (2012) and older CPUs (e.g., Mac Mini 2012). This avoids FMA3 instructions that cause SIGILL on pre-Haswell processors.

For maximum performance on a known-modern host (Haswell or newer), set `FUJISAN_NATIVE_CPU=1` before building:

```bash
FUJISAN_NATIVE_CPU=1 ./build.sh linux
```

Or override with explicit CFLAGS:

```bash
CFLAGS="-O3 -march=native -mtune=native" ./build.sh linux
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