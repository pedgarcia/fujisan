# Linux ARM64 Build Support - Requirements and Implementation Plan

## Current State

### Existing Linux Build System
- Uses **Ubuntu 22.04 container** via Docker/Podman
- Platform: `--platform linux/amd64` (x86_64 only)
- Scripts: `build.sh` calls `scripts/build-linux-docker.sh`
- Dockerfile: `docker/Dockerfile.ubuntu-22.04`
- Outputs:
  - `fujisan_{version}_amd64.deb` (Debian package)
  - `fujisan-{version}-linux-x64.tar.gz` (portable tarball)

### Issue Found: SDL2 Missing from Dockerfile
**Critical**: SDL2 libraries are NOT installed in the Ubuntu container, which means joystick support won't work on Linux builds!

Need to add:
- `libsdl2-dev` (development headers)
- `libsdl2-2.0-0` (runtime library)

## Target Architecture

### ARM64/aarch64 (Recommended)
- 64-bit ARM architecture
- Modern standard for:
  - Raspberry Pi 4, Pi 5
  - AWS Graviton instances
  - Modern ARM servers and devices
  - Apple Silicon development (native!)

### ARMv7 (Not Recommended)
- 32-bit ARM (legacy)
- Raspberry Pi 3 and older
- Declining market share
- Can skip for now

## Technical Approach

### Option A: Native ARM64 Container (RECOMMENDED)

**How it works:**
- Use same Ubuntu 22.04 base image
- Specify `--platform linux/arm64` instead of `linux/amd64`
- Docker/Podman automatically pulls ARM64 versions of packages
- All existing dependencies work on ARM64

**Performance:**
- **On Apple Silicon Mac (your system)**: Native speed! ARM64 runs natively
- **On Intel Mac**: 5-10x slower (QEMU emulation)
- **On Linux x86_64**: Emulated via QEMU

**Advantages:**
- Simple - reuse existing Dockerfile
- Proven approach - same as macOS universal builds
- Reliable - Ubuntu packages well-maintained for ARM64
- Fast on your Apple Silicon Mac

**Disadvantages:**
- Slower on Intel Macs (but acceptable for CI/CD)

### Option B: Cross-compilation from x86_64 (NOT RECOMMENDED)

**How it works:**
- Install ARM64 cross-compiler toolchain
- Cross-compile from x86_64 host
- More complex CMake configuration

**Advantages:**
- Faster on Intel hosts

**Disadvantages:**
- Complex setup and maintenance
- More prone to errors
- Overkill for this use case
- You have Apple Silicon anyway!

## Implementation Plan

### 1. Fix SDL2 Missing in Dockerfile

**File:** `docker/Dockerfile.ubuntu-22.04`

**Change:** Add SDL2 packages to apt-get install:
```dockerfile
RUN apt-get update && apt-get install -y \
    ...existing packages...
    # SDL2 for joystick support
    libsdl2-dev \
    libsdl2-2.0-0 \
    && rm -rf /var/lib/apt/lists/*
```

**Impact:** Affects both x86_64 and ARM64 builds (bug fix!)

### 2. Update build.sh

**File:** `build.sh`

**Add platform options:**
- `linux-x86_64` or `linux-amd64` - explicit x86_64 build
- `linux-arm64` or `linux-aarch64` - ARM64 build
- `linux` - default to x86_64 for backward compatibility

**Implementation:**
```bash
case "$PLATFORM" in
    linux|linux-x86_64|linux-amd64)
        build_linux "amd64"
        ;;
    linux-arm64|linux-aarch64)
        build_linux "arm64"
        ;;
    ...
esac
```

### 3. Extend build-linux-docker.sh

**File:** `scripts/build-linux-docker.sh`

**Add architecture parameter:**
```bash
# Architecture (default to amd64 for backward compatibility)
ARCH="${1:-amd64}"

# Validate architecture
if [[ "$ARCH" != "amd64" && "$ARCH" != "arm64" ]]; then
    echo_error "Invalid architecture: $ARCH (must be amd64 or arm64)"
    exit 1
fi
```

**Update platform flag:**
```bash
# Build container
$CONTAINER_RUNTIME build \
    --platform linux/$ARCH \
    -f docker/Dockerfile.ubuntu-22.04 \
    -t fujisan-linux-builder:ubuntu22-$ARCH \
    .
```

**Update output naming:**
```bash
# Debian package
DEB_PACKAGE="fujisan_${VERSION_CLEAN}_${ARCH}.deb"

# Tarball
if [[ "$ARCH" == "amd64" ]]; then
    TARBALL="fujisan-${VERSION_CLEAN}-linux-x64.tar.gz"
else
    TARBALL="fujisan-${VERSION_CLEAN}-linux-${ARCH}.tar.gz"
fi
```

### 4. No Code Changes Required

**CMakeLists.txt:** Already portable, no ARM-specific changes needed
**libatari800:** Pure C code, compiles on ARM64 without modification
**Qt5:** Has native ARM64 packages in Ubuntu repos
**SDL2:** Has native ARM64 packages in Ubuntu repos

## Build Commands

### Building for ARM64
```bash
# Single architecture
./build.sh linux-arm64 --version v1.0.0

# With options
./build.sh linux-arm64 --clean --version v1.0.0
```

### Building for x86_64 (backward compatible)
```bash
# Existing command still works
./build.sh linux --version v1.0.0

# Explicit
./build.sh linux-x86_64 --version v1.0.0
```

### Building both architectures
```bash
./build.sh linux-x86_64 --version v1.0.0
./build.sh linux-arm64 --version v1.0.0
```

## Expected Outputs

### ARM64 Build
- `dist/linux/fujisan_{version}_arm64.deb`
- `dist/linux/fujisan-{version}-linux-arm64.tar.gz`

### x86_64 Build (unchanged)
- `dist/linux/fujisan_{version}_amd64.deb`
- `dist/linux/fujisan-{version}-linux-x64.tar.gz`

## Testing

### Before Release
1. **Build verification**: Both architectures compile successfully
2. **Package integrity**: .deb files install correctly
3. **Tarball verification**: Portable packages extract and run

### Functional Testing Options

**Virtual Machine:**
- Lima (lightweight Linux VMs on macOS)
- UTM (ARM64 Linux on Apple Silicon)
- Parallels Desktop (if available)

**Real Hardware:**
- Raspberry Pi 4 or 5 (Ubuntu 22.04+)
- Any ARM64 Linux device

**Cloud:**
- AWS EC2 Graviton instance (t4g.micro for testing)
- Oracle Cloud ARM instances (free tier available)

### Test Checklist
- [ ] Application launches
- [ ] Joystick detection works (USB controller)
- [ ] ROM loading functions
- [ ] Disk images mount
- [ ] Audio playback works
- [ ] Keyboard input responsive
- [ ] Settings persist correctly

## Performance Considerations

### Build Time on Apple Silicon Mac
- **x86_64 build**: ~5-10 minutes (emulated via Rosetta/QEMU)
- **ARM64 build**: ~2-5 minutes (NATIVE - no emulation!)
- **Parallel builds**: Can run both simultaneously

### Build Time on Intel Mac
- **x86_64 build**: ~2-5 minutes (native)
- **ARM64 build**: ~15-30 minutes (QEMU emulation, slow)

### Recommendation
If building on Intel Mac for CI/CD, consider:
- Use cloud ARM64 runners (GitHub Actions has ARM64 runners)
- Build ARM64 on separate ARM64 machine
- Accept slower emulated builds (still works, just slower)

## Deployment Targets

### Raspberry Pi
- Pi 4 Model B (4GB/8GB) - Recommended
- Pi 5 - Best performance
- Pi 3 - Would need ARMv7 build (not included)

### ARM Servers
- AWS Graviton (EC2, ECS, Lambda)
- Oracle Cloud Ampere
- Azure ARM VMs
- Packet/Equinix Metal

### Consumer Devices
- ARM Chromebooks running Linux
- ARM-based Linux laptops
- Single-board computers (Rock Pi, Orange Pi, etc.)

## Future Enhancements

### Multi-arch Container Images
Could create multi-architecture Docker images:
```bash
docker buildx build --platform linux/amd64,linux/arm64 ...
```

### AppImage Support
Current x86_64 AppImage could be extended to ARM64:
- Use `appimagetool-aarch64.AppImage`
- Same approach as x86_64 build

### Snap Package
Universal package format supporting both architectures:
```bash
snapcraft --target-arch=arm64
snapcraft --target-arch=amd64
```

## Known Limitations

1. **No ARMv7 (32-bit) support** - Only ARM64 included
2. **Emulation speed** - Slow on non-native architectures
3. **Testing coverage** - Manual testing needed for ARM64

## References

- Ubuntu ARM64 packages: https://packages.ubuntu.com/jammy/arm64/
- Docker multi-platform: https://docs.docker.com/build/building/multi-platform/
- Qt ARM support: https://doc.qt.io/qt-5/embedded-linux.html
- Raspberry Pi OS: https://www.raspberrypi.com/software/

## Summary

Adding Linux ARM64 support is **straightforward** because:
1. ✅ You have Apple Silicon Mac (native ARM64 builds!)
2. ✅ Existing codebase is portable
3. ✅ Ubuntu has excellent ARM64 package support
4. ✅ Same build approach as macOS universal builds
5. ⚠️ Need to fix missing SDL2 in Dockerfile (affects all Linux builds)

**Recommended next steps:**
1. Add SDL2 to Dockerfile (critical bug fix)
2. Implement architecture parameter in build scripts
3. Test ARM64 build on your Mac (should be fast!)
4. Verify on Raspberry Pi or cloud ARM instance
