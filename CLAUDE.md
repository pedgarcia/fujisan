# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fujisan is a modern Qt5-based frontend for the Atari800 emulator that provides a native desktop experience with full keyboard support, machine configuration, and authentic Atari behavior. It uses libatari800 as its core emulator library and includes comprehensive debugging, TCP server API, and media management capabilities.

## Build System and Common Commands

### Prerequisites Setup
```bash
# Set required environment variable (needed for all build operations)
export ATARI800_SRC_PATH=/path/to/atari800-src

# macOS: Install Qt5 via Homebrew
brew install qt@5 cmake autoconf automake

# Linux: Install Qt5 development packages
sudo apt install qtbase5-dev cmake build-essential autoconf automake
```

### Build Process
```bash
# 1. Apply Fujisan patches to atari800 source
cd fujisan/patches
./apply-patches.sh

# 2. Build libatari800 (required dependency)
cd $ATARI800_SRC_PATH
./autogen.sh              # Generate configure script if needed
./configure --target=libatari800
make

# 3. Build Fujisan (out-of-source build recommended)
cd fujisan
mkdir -p build && cd build
ATARI800_SRC_PATH=/path/to/atari800-src cmake ..
# macOS with Homebrew Qt5:
# ATARI800_SRC_PATH=/path/to/atari800-src CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake ..
make

# 4. Run the emulator
./fujisan
```

### Patch System
Fujisan uses Git format-patch files for reliable patching:
- **0001-libatari800-disk-api.patch**: Core disk management API additions
- Patches are compatible with atari800 commit `575a943b3523bf0f5c9b73ba8641d83cac672f24`
- Use `apply-patches.sh` which auto-detects Git repos and uses appropriate commands
- Patches add essential libatari800 API functions for disk management and activity monitoring

### macOS Universal Binary Build
```bash
# Prerequisites: Install Qt5 for both architectures
# ARM64 (Apple Silicon):
brew install qt@5

# x86_64 (Intel) - requires Rosetta:
arch -x86_64 /usr/local/bin/brew install qt@5

# Build universal binary with universal Qt frameworks
./scripts/build-universal-macos-complete.sh

# Options:
#   --clean       Clean build directories before starting
#   --skip-arm64  Skip ARM64 build (use existing)
#   --skip-x86_64 Skip x86_64 build (use existing)
#   --sign        Sign the application (requires certificates)

# Output: dist/Fujisan-{version}-universal.dmg
# The DMG contains a universal app bundle that runs natively on both Intel and Apple Silicon Macs
```

### Windows Cross-Compilation (from macOS/Linux)
```bash
# Prerequisites: Install Podman or Docker
# macOS: brew install podman (or docker)
# Linux: sudo apt install podman (or docker)

# 1. Build Windows executable using MinGW container
./scripts/build-windows-cross.sh
# Note: Script uses Podman by default. For Docker, edit the script 
# and replace 'podman' with 'docker'

# 2. Create Windows release package with all dependencies
./create-windows-release-with-audio.sh
# This script also uses Podman; replace with Docker if needed

# The complete Windows release will be in build-windows/
# Includes Qt5 DLLs, audio plugins, and platform plugins
```

### Clean Build
```bash
# Clean out-of-source build
rm -rf build/

# Clean in-source build artifacts
rm -rf CMakeCache.txt CMakeFiles/ Makefile cmake_install.cmake Fujisan_autogen/

# Clean Windows cross-compilation
rm -rf build-cross-windows/ build-windows/
```

## Code Architecture

### Core Components
- **AtariEmulator** (`src/atariemulator.cpp`): Core emulator integration layer that interfaces with libatari800, handles input conversion, audio, disk I/O monitoring, and state management
- **EmulatorWidget** (`src/emulatorwidget.cpp`): Qt widget responsible for display rendering, keyboard event capture, and drag-and-drop functionality
- **MainWindow** (`src/mainwindow.cpp`): Main application window with menu system, toolbar controls, media management, and overall application coordination
- **DebuggerWidget** (`src/debuggerwidget.cpp`): Comprehensive 6502 debugging interface with breakpoints, memory inspection, disassembly, and execution control
- **TCPServer** (`src/tcpserver.cpp`): JSON-based TCP API server (port 8080) providing complete remote control capabilities for IDE integration and automation

### Key Design Patterns
- **Separation of Concerns**: Emulator logic (AtariEmulator) is completely separate from UI (EmulatorWidget, MainWindow)
- **Signal-Slot Architecture**: Qt signals/slots used for loose coupling between components (disk activity, frame updates, configuration changes)
- **libatari800 Integration**: Direct C library integration with proper memory management and input conversion
- **Configuration Management**: Profile-based system with automatic restart handling for settings that require emulator reinitialization

### Input System
- **Keyboard Mapping**: Qt key events converted to AKEY constants for authentic Atari behavior
- **Joystick Emulation**: Configurable keyboard-to-joystick mapping (numpad/WASD) with swap support
- **Input Injection**: Direct character injection system for paste functionality and TCP automation

### Display Pipeline
1. libatari800 generates 384x240 screen buffer with color indices
2. Color indices converted to RGB using Colours_table from libatari800
3. QImage updated with converted pixels
4. QPainter scales and displays with proper aspect ratio (98% of available space)
5. Real-time updates at proper Atari timing (49.86 FPS PAL / 59.92 FPS NTSC)

### Media Management Architecture
- **Disk Drives**: Support for D1-D8 with individual enable/disable, activity monitoring via SIO detection
- **Cartridges**: Hot-swappable cartridge support with automatic restart handling
- **State Persistence**: All media states saved to QSettings for session restoration

## Development Guidelines

### libatari800 Integration
- Always use libatari800 API functions rather than direct memory access when available
- Handle emulator initialization failures gracefully with proper error messages
- Respect emulator timing constraints - use frame-based updates, not arbitrary timers
- Memory access must be done through libatari800_get_main_memory_ptr() for safety

### Qt Best Practices
- Use Qt's signal-slot system for component communication
- Implement proper event filtering in MainWindow for global key handling
- Use QSettings for persistent configuration with proper key organization
- Handle window lifecycle events (closeEvent, resizeEvent) for proper shutdown

### Configuration System
- All settings stored in ConfigurationProfile objects for consistency
- Settings that require emulator restart should be flagged appropriately
- Use ConfigurationProfileManager for profile operations and validation
- Apply settings in proper order: basic → machine → video → input → advanced

### TCP Server Development
- All commands follow JSON format: {"command": "category.action", "params": {...}}
- Validate all file paths and parameters before passing to emulator
- Use proper error responses with descriptive messages
- Broadcast events to all connected clients for state changes
- Commands are organized by category: media, system, input, debug, config, status

### Debugging Infrastructure
- DebuggerWidget integrates with libatari800's CPU state access
- Breakpoints managed through internal system with visual feedback
- Memory inspection uses proper 6502 addressing and display formats
- Step operations work at instruction level using libatari800 controls

### NetSIO/FujiNet Support
Fujisan includes comprehensive FujiNet-PC network connectivity:
- **Local Disk Priority**: Local mounted disks (D1-D8) take precedence over FujiNet devices
- **SIO Integration**: Proper command routing between local SIO and NetSIO systems  
- **Automatic Configuration**: BASIC auto-disabled when NetSIO enabled (required for FujiNet boot)
- **Delayed Restart**: 60-frame delay after NetSIO initialization for proper boot sequence
- **Command Synchronization**: Uses sync mechanisms for reliable FujiNet communication

### Testing Considerations
- Test with different machine types (400/800, 800XL, 130XE, 5200)
- Verify both NTSC and PAL timing behavior
- Test with and without BASIC ROM enabled
- Validate disk operations across all 8 drives
- Test TCP server with multiple concurrent clients
- Test NetSIO/FujiNet connectivity with FujiNet-PC running on port 9997
- Verify local disk priority over network devices

## Build and Release Organization

### Platform-Specific Build Folders
- **macOS**: 
  - `build/` - Native single-architecture build
  - `build-arm64/` - ARM64 build for universal binary
  - `build-x86_64/` - x86_64 build for universal binary
  - `build-universal/` - Universal binary with both architectures
  - `dist/` - Final DMG distribution files
- **Windows**: `build-windows/` (cross-compiled release package with all DLLs)
- **Linux**: `build/` (native build)

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

### Build Artifacts (.gitignore)
- `build/` - Native build directory
- `build-arm64/` - macOS ARM64 build directory
- `build-x86_64/` - macOS x86_64 build directory
- `build-universal/` - macOS universal binary directory
- `build-cross-windows/` - Windows cross-compilation temp directory
- `build-windows/` - Windows release package
- `dist/` - Distribution files (DMGs, etc.)
- `*.exe`, `*.dll` - Binary files
- `*.dmg` - macOS disk images 