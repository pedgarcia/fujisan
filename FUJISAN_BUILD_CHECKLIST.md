# Fujisan Build Checklist

This checklist MUST be followed for every build to prevent regression of fixed bugs.
Keep this document up to date as new critical fixes are added.

## Pre-Build Verification

- [ ] Verify all patches in `patches/` directory are present:
  - [ ] `0001-libatari800-disk-api.patch`
  - [ ] `0002-windows-ulong-conflicts.patch`
  - [ ] `0003-disk-activity-callback-integration.patch`
- [ ] Confirm `patches/apply-patches.sh` has execute permissions
- [ ] Check that CMakeLists.txt includes PATCH_COMMAND for ExternalProject
- [ ] Clean previous build directories if switching architectures

## Critical Function Declarations

These MUST be present in the code for features to work:

- [ ] `extern void libatari800_set_disk_activity_callback(...)` in `include/atariemulator.h`
- [ ] SDL2 conditionally compiled with `#ifdef HAVE_SDL2_AUDIO`
- [ ] Dynamic speed adjustment members in AtariEmulator class
- [ ] Joystick keyboard emulation checks main joystick enable state

## Platform-Specific Build Configuration

### macOS ARM64 (Apple Silicon)
- [ ] SDL2 enabled (default)
- [ ] Qt5 from `/opt/homebrew/opt/qt@5`
- [ ] Build directory: `build-arm64/`
- [ ] Command: `cmake -DCMAKE_OSX_ARCHITECTURES=arm64 ...`

### macOS x86_64 (Intel)
- [ ] SDL2 DISABLED: `-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE`
- [ ] Qt5 from `/usr/local/opt/qt@5`
- [ ] Build directory: `build-x86_64/`
- [ ] Command includes SDL2 disable flag

### Windows (Cross-compilation)
- [ ] SDL2 DISABLED: `-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE`
- [ ] Using MinGW-w64 cross-compiler
- [ ] Container-based build with podman/docker
- [ ] Scripts: `build-windows-simple.sh` or `build-windows-cross.sh`

### Linux (Container build)
- [ ] SDL2 DISABLED: `-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE`
- [ ] Container-based build with podman/docker
- [ ] Ubuntu 24.04 base image
- [ ] Scripts: `build-linux-docker.sh` or `build-linux-local.sh`

## Patch Application Verification

After configure step, verify patches were applied:

- [ ] Check build logs for "Applying Fujisan patches..."
- [ ] Verify each patch message: "Applying patch: 000X-*.patch"
- [ ] Check for `libatari800_set_disk_activity_callback` in:
  - `build-*/atari800-src/src/libatari800/api.c`
  - `build-*/atari800-src/src/libatari800/libatari800.h`

## Audio System Verification

- [ ] Qt audio double buffering enabled
- [ ] Dynamic speed adjustment (0.95-1.05) implemented
- [ ] Buffer monitoring without debug logs in production
- [ ] SDL2 only on macOS ARM64 builds

## Joystick Settings Verification

- [ ] Main joystick checkbox controls keyboard joystick emulation
- [ ] Settings dialog respects main joystick state
- [ ] Toolbar updates apply effective joystick state
- [ ] Keyboard joystick checkboxes disabled when main joystick disabled

## Post-Build Testing

### Code Signing Verification (macOS Distribution Builds)
- [ ] DMGs created with `--sign` flag
- [ ] Developer ID Application certificate detected automatically
- [ ] Hardened runtime enabled on all binaries
- [ ] All frameworks and plugins signed
- [ ] No ad-hoc signatures in distribution build

```bash
# Verify signing
codesign --verify --deep --strict dist/macos/Fujisan-*-arm64.dmg
codesign --display --verbose=2 /Volumes/Fujisan*/Fujisan.app
```

### Disk LED Activity
- [ ] Mount a disk image
- [ ] Observe LED turns on during disk activity
- [ ] LED turns off when activity completes
- [ ] Both read and write operations trigger LED

### Audio Quality
- [ ] No audio dropouts during normal operation
- [ ] Dynamic speed adjustment active (check logs if needed)
- [ ] Audio continues smoothly during disk operations

### Joystick Functionality
- [ ] Keyboard joystick only works when main joystick enabled
- [ ] Settings changes apply immediately
- [ ] WASD/Numpad keys only captured when appropriate

### Platform Builds
- [ ] macOS ARM64 builds and runs
- [ ] macOS x86_64 builds and runs
- [ ] Windows .exe created successfully
- [ ] Linux binary/packages created

### Distribution Readiness (macOS)
- [ ] DMGs signed with Developer ID (not ad-hoc)
- [ ] Notarization successful
- [ ] DMGs stapled with notarization
- [ ] No security warnings when opening on clean Mac

## Common Issues and Solutions

### Issue: Disk LED not working
- **Check**: Is `libatari800_set_disk_activity_callback` declared?
- **Check**: Were patches applied during build?
- **Solution**: Clean build, verify patch application

### Issue: Audio dropouts
- **Check**: Is dynamic speed adjustment implemented?
- **Check**: Are buffer sizes appropriate for sample rate?
- **Solution**: Verify speed adjustment code in processFrame()

### Issue: Windows/Linux build failures with SDL2
- **Check**: Is SDL2 disabled with `-DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE`?
- **Solution**: Update build scripts to disable SDL2

### Issue: Joystick keys always captured
- **Check**: Does settings dialog respect main joystick state?
- **Check**: Does toolbar update apply effective state?
- **Solution**: Verify joystick state logic in both locations

## Build Commands Quick Reference

```bash
# macOS both architectures (development)
./build.sh macos

# macOS both architectures (distribution)
./build.sh macos --sign

# macOS ARM64 only (development)
./scripts/build-macos-separate-dmgs.sh --skip-x86_64

# macOS ARM64 only (distribution)
./scripts/build-macos-separate-dmgs.sh --skip-x86_64 --sign

# macOS x86_64 only (distribution)
./scripts/build-macos-separate-dmgs.sh --skip-arm64 --sign

# Windows
./build.sh windows

# Linux
./build.sh linux

# All platforms (with macOS signing)
./build.sh all --sign

# Notarize signed DMGs
./scripts/sign-and-notarize-dmgs.sh --skip-signing
```

## Maintenance Notes

- Update this checklist when adding new critical fixes
- Add new patches to the patch list
- Document any new platform-specific requirements
- Keep build script references current