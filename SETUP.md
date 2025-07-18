# Fujisan - Modern Atari Emulator

A modern Qt5 frontend for the Atari800 emulator providing native desktop integration with full keyboard support, machine configuration, and authentic Atari experience.

## Overview

This Qt5 GUI provides a complete alternative to the built-in SDL interface, offering:
- **Native desktop menus and dialogs**
- **Full keyboard input** (including CAPS LOCK, shifted symbols, function keys)
- **Machine type selection** (400/800, 800XL, 130XE, 5200)
- **Video system configuration** (NTSC/PAL)
- **ROM management** (Original Atari vs Altirra ROMs)
- **BASIC toggle and system controls**
- **Authentic Atari colors and timing**

## Prerequisites

### All Platforms
- **CMake 3.16 or higher**
- **C++17 compatible compiler**
- **Built libatari800.a** (from parent atari800 source)

### Platform-Specific Dependencies

#### **macOS**
```bash
# Install Qt5 via Homebrew
brew install qt@5

# Or install Qt5 via official installer
# Download from: https://www.qt.io/download-qt-installer
```

#### **Linux (Ubuntu/Debian)**
```bash
# Install Qt5 development packages
sudo apt update
sudo apt install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools

# Install CMake if not available
sudo apt install cmake build-essential
```

#### **Linux (CentOS/RHEL/Fedora)**
```bash
# For CentOS/RHEL
sudo yum install qt5-qtbase-devel cmake gcc-c++

# For Fedora
sudo dnf install qt5-qtbase-devel cmake gcc-c++
```

#### **Windows**
1. **Install Qt5**: Download and install Qt5 from [qt.io](https://www.qt.io/download-qt-installer)
   - Choose Qt 5.15.x or later
   - Select "Qt 5.x.x" → "MSVC 2019 64-bit" (or MinGW if preferred)

2. **Install CMake**: Download from [cmake.org](https://cmake.org/download/)

3. **Install Visual Studio 2019 or later** (or MinGW if using MinGW Qt)

## Installation

### 1. Add to Existing Atari800 Source

Copy or clone this `fujisan` folder into your existing atari800 source tree:

```
atari800-src/
├── src/                  # Original atari800 source
├── DOC/                  # Documentation
├── fujisan/              # ← This Qt GUI folder
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   └── SETUP.md
└── ... (other atari800 files)
```

### 2. Build libatari800 (Required First)

The Qt GUI requires `libatari800.a` to be built first:

```bash
# Navigate to atari800 root directory
cd /path/to/atari800-src

# Configure for libatari800 target
./configure --target=libatari800

# Build the library
make
```

Verify that `src/libatari800.a` exists after building.

### 3. Build Qt GUI

#### **macOS / Linux**
```bash
# Navigate to Fujisan directory
cd fujisan

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make

# Binary location: ./fujisan
```

#### **Windows (Visual Studio)**
```batch
REM Navigate to Fujisan directory
cd fujisan

REM Create build directory
mkdir build
cd build

REM Configure with CMake (adjust Qt path as needed)
cmake -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64" ..

REM Build
cmake --build . --config Release

REM Binary location: Release\fujisan.exe
```

#### **Windows (MinGW)**
```batch
REM Navigate to Fujisan directory
cd fujisan

REM Create build directory
mkdir build
cd build

REM Configure with CMake (adjust Qt path as needed)
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\mingw81_64" ..

REM Build
mingw32-make

REM Binary location: fujisan.exe
```

## Binary Location

After successful build, the executable will be located at:

- **macOS/Linux**: `fujisan/build/fujisan`
- **Windows**: `fujisan/build/Release/fujisan.exe` (or `fujisan/build/fujisan.exe` for MinGW)

## ROM Configuration

### Using Original Atari ROMs

Place your Atari ROM files in `/Applications/Emulators/roms/atari/` (macOS) or equivalent:

- `atarixl.rom` - Atari XL/XE Operating System
- `ataribas.rom` - Atari BASIC ROM
- `atari5200.rom` - Atari 5200 BIOS (optional)

**Note**: You must legally obtain these ROMs (e.g., from PC Xformer 2.5 package).

### Using Built-in Altirra ROMs

The emulator includes free Altirra replacement ROMs. Enable via:
**System** → **Use Altirra OS** (checked)

## Usage

### Running the Emulator
```bash
# macOS/Linux
./fujisan

# Windows
fujisan.exe
```

### Key Features

#### **System Menu**
- **Enable BASIC**: Toggle BASIC ROM loading
- **Use Altirra OS**: Switch between original and Altirra ROMs
- **Auto-restart on changes**: Automatically restart when settings change
- **Restart with Current Settings**: Manual restart

#### **Machine Menu**
- **Machine Types**: Atari 400/800, 800XL, 130XE, 5200
- **Video Systems**: NTSC (59.92 fps), PAL (49.86 fps)

#### **Function Keys**
- **F2**: Start button
- **F3**: Select button
- **F4**: Option button
- **F5**: Warm Reset
- **Shift+F5**: Cold Reset
- **F6**: Help key
- **F7/Pause**: Break key

#### **Keyboard Support**
- **Full alphabet**: A-Z with proper case handling
- **CAPS LOCK**: Toggle between upper/lowercase (inverted like real Atari)
- **Shifted symbols**: All !@#$%^&*() and punctuation
- **Special keys**: Enter, arrows, function keys, Escape, Backspace

## Troubleshooting

### Common Issues

#### **"libatari800 not found"**
```
FATAL_ERROR: libatari800 not found. Please build it first with: cd .. && ./configure --target=libatari800 && make
```
**Solution**: Build `libatari800.a` first as described in step 2.

#### **Qt5 not found**
```
Could NOT find Qt5 (missing: Qt5_DIR)
```
**Solution**: 
- Install Qt5 development packages
- Set `CMAKE_PREFIX_PATH` to Qt5 installation directory
- Example: `cmake -DCMAKE_PREFIX_PATH="/usr/local/opt/qt@5" ..`

#### **Missing ROM files**
```
Failed to initialize emulator with: atari800 -xl -pal -xlxe_rom /path/atarixl.rom
```
**Solution**: 
- Place ROM files in correct directory
- Or enable "Use Altirra OS" for built-in ROMs

#### **Compiler errors**
```
error: 'AKEY_a' was not declared in this scope
```
**Solution**: Ensure proper include paths for atari800 headers in CMakeLists.txt

### Platform-Specific Notes

#### **macOS**
- Use `brew install qt@5` for easy Qt5 installation
- May need to add Qt5 to PATH: `export PATH="/usr/local/opt/qt@5/bin:$PATH"`

#### **Linux**
- Install development packages (`-dev` or `-devel`)
- Ensure CMake can find Qt5: `export CMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5`

#### **Windows**
- Ensure Qt5 bin directory is in PATH for runtime
- Visual Studio 2019 or later recommended
- Set `CMAKE_PREFIX_PATH` to exact Qt installation path

## Contributing

This Qt frontend maintains compatibility with the core atari800 emulator while providing modern desktop integration. When modifying:

1. **Preserve libatari800 API compatibility**
2. **Follow Qt5 best practices** for cross-platform support
3. **Test on multiple platforms** before committing
4. **Maintain authentic Atari behavior** (timing, colors, keyboard mapping)

## License

This Qt frontend follows the same license as the main atari800 project. See `COPYING` in the root directory.