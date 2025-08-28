# Fujisan 1.0.1 Release Notes

## Overview
Fujisan 1.0.1 is a maintenance release that significantly improves audio performance, debugger functionality, and cross-platform compatibility. This release includes critical fixes for Windows audio, enhanced debugging precision, and improved build system reliability.

## Key Improvements

### Audio System Enhancements
- **New SDL2 Audio Backend**: Implemented a robust ring buffer-based audio system for improved stability
- **Windows Audio Optimization**: Fixed audio dropouts and crackling issues on Windows platforms
- **Dynamic Speed Adjustment**: Added intelligent audio synchronization to prevent buffer underruns
- **Double Buffering**: Implemented double buffering inspired by Atari800MacX for smoother playback

### Debugger Improvements
- **Instruction-Level Precision**: Breakpoints now trigger at exact instruction boundaries
- **Enhanced Step Commands**: Fixed `step_over` to properly handle single-instruction stepping
- **Partial Frame Execution**: Improved debugging accuracy with partial frame execution support
- **Unified Debugging Interface**: Consolidated breakpoint handling for better reliability

### Platform-Specific Fixes

#### macOS
- Fixed architecture-specific builds for both Intel (x86_64) and Apple Silicon (ARM64)
- Improved dark mode UI compatibility
- Fixed missing disk LED indicator
- Resolved Qt5 library conflicts between architectures

#### Windows
- Fixed cross-compilation build issues
- Optimized audio buffer sizes to prevent dropouts
- Improved overall stability and performance

#### Linux
- Enhanced container build system for better cross-platform compatibility
- Fixed distribution packaging issues

### UI/UX Improvements
- Fixed dark mode rendering issues across all platforms
- Restored disk activity LED functionality
- Improved joystick configuration interface
- Fixed missing UI images and icons

### Build System
- Improved patch application system for libatari800
- Enhanced build scripts for all platforms
- Better error handling and recovery in build process
- Organized distribution files into platform-specific subdirectories

## Bug Fixes
- Fixed disk LED activity indicator not showing disk operations
- Resolved joystick settings persistence issues
- Fixed various UI elements not displaying correctly in dark mode
- Corrected build script issues for Windows cross-compilation
- Fixed patch application failures in certain environments

## Technical Details
- Removed excessive debug logging from production builds
- Improved memory management in audio subsystem
- Enhanced error handling throughout the application
- Better integration with libatari800 core

## Known Issues
- XEX load-without-run functionality is experimental and will be refined in future releases

## Compatibility
- macOS: 10.13+ (Intel and Apple Silicon native)
- Windows: Windows 10/11 (64-bit)
- Linux: Ubuntu 20.04+, Debian 11+, and compatible distributions

## Downloads
All platform builds are available in the `dist/` directory:
- `Fujisan-1.0.1-arm64.dmg` - macOS Apple Silicon
- `Fujisan-1.0.1-x86_64.dmg` - macOS Intel
- `Fujisan-1.0.1-windows.zip` - Windows 64-bit
- `fujisan-1.0.1-linux-x64.tar.gz` - Linux x64
- `fujisan_1.0.1_amd64.deb` - Debian/Ubuntu package

## Acknowledgments
Thank you to all users who reported issues and provided feedback that helped improve this release.

---
*For installation instructions and documentation, please refer to the README.md file.*