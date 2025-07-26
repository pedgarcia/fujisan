# Fujisan - Modern Atari Emulator

A modern Qt5 frontend for the Atari800 emulator, providing a native desktop experience with full keyboard support, machine configuration, and authentic Atari behavior.

![Fujisan Screenshot](images/screenshot1.png)

## Features

### Emulator Integration
- **libatari800 Integration**: Uses the same proven emulator core
- **Authentic Display**: 384x240 full screen resolution with proper Atari colors
- **Pixel Perfect Scaling**: Integer scaling for crisp, retro graphics
- **Real-time Performance**: Proper 49.86 FPS (PAL) / 59.92 FPS (NTSC) timing

### User Interface
- **Native Qt5 Menus**: Standard desktop menu bar and dialogs
- **File Management**: Native file dialogs for ROM loading
- **Status Bar**: Real-time feedback for user actions
- **Focus Management**: Click-to-focus emulator display

### Keyboard Input
- **Full Keyboard Support**: All letters, numbers, and symbols
- **Shifted Symbols**: Proper handling of `!@#$%^&*()` and punctuation
- **Special Keys**: Enter, arrows, function keys, Escape, Backspace
- **Control Combinations**: Ctrl+C for break, Ctrl+letters for control codes
- **Real-time Response**: Direct input to emulator without lag

### System Control
- **BASIC Toggle**: Enable/disable BASIC ROM loading
- **Cold/Warm Boot**: System restart options
- **Dynamic Restart**: Apply BASIC settings without restarting GUI
- **ROM Loading**: Support for .rom, .bin, .car, .atr files

### Technical Features
- **Color Accuracy**: Uses actual Atari color table from libatari800
- **Memory Efficiency**: Direct screen buffer access
- **Cross-platform**: Qt5 ensures compatibility across desktop platforms
- **Clean Architecture**: Separated emulator core, display widget, and UI

## Building

### Prerequisites
- **Qt5** (Core, Widgets, Gui modules)
- **CMake 3.16+**
- **C++17 compatible compiler**
- **libatari800** (built in parent atari800 source directory)

### Platform-Specific Setup

#### **macOS**
```bash
# Install Qt5 via Homebrew (recommended)
brew install qt@5

# Install CMake if not available
brew install cmake

# Add Qt5 to PATH (if needed)
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
```

#### **Linux (Ubuntu/Debian)**
```bash
# Install Qt5 development packages
sudo apt update
sudo apt install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools

# Install CMake and build tools
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
1. **Install Qt5**: Download from [qt.io](https://www.qt.io/download-qt-installer)
   - Choose Qt 5.15.x or later
   - Select "MSVC 2019 64-bit" or "MinGW" kit
2. **Install CMake**: Download from [cmake.org](https://cmake.org/download/)
3. **Install Visual Studio 2019+** (for MSVC) or **MinGW** (for GCC)

### Build Steps

#### **Step 1: Build libatari800** (Required)
```bash
# Navigate to atari800 source root
cd /path/to/atari800-src

# Configure for library target
./configure --target=libatari800

# Build the library
make

# Verify libatari800.a exists
ls -la src/libatari800.a
```

#### **Step 2: Build Fujisan**

**macOS/Linux:**
```bash
# Navigate to Fujisan directory
cd fujisan

# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake (macOS may need Qt5 path)
CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake ..
# OR for Linux typically just:
# cmake ..

# Build
make

# Run
./fujisan
```

**Windows (Visual Studio):**
```batch
REM Navigate to Fujisan directory
cd fujisan

REM Create build directory
mkdir build
cd build

REM Configure (adjust Qt path as needed)
cmake -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64" ..

REM Build
cmake --build . --config Release

REM Run
Release\fujisan.exe
```

**Windows (MinGW):**
```batch
REM Navigate to Fujisan directory  
cd fujisan

REM Create build directory
mkdir build
cd build

REM Configure
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\mingw81_64" ..

REM Build
mingw32-make

REM Run
fujisan.exe
```

### Troubleshooting Build Issues

#### **"Qt5 not found" Error**
```
Could NOT find Qt5 (missing: Qt5_DIR)
```
**Solution**: Set the correct Qt5 path:
- **macOS**: `CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake ..`
- **Linux**: Install Qt5 dev packages or set path
- **Windows**: `cmake -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64" ..`

#### **"libatari800 not found" Error**
```
FATAL_ERROR: libatari800 not found
```
**Solution**: Build libatari800 first:
```bash
cd /path/to/atari800-src
./configure --target=libatari800
make
```

#### **Missing Build Tools**
- **macOS**: Install Xcode Command Line Tools: `xcode-select --install`
- **Linux**: Install build-essential: `sudo apt install build-essential`
- **Windows**: Install Visual Studio with C++ tools

### Running
```bash
# macOS/Linux
./fujisan

# Windows
fujisan.exe
```

## Usage

### Getting Started
1. Launch the application
2. The emulator starts with BASIC enabled by default
3. Click on the display area to focus keyboard input
4. Type BASIC commands and press Enter

### Loading ROMs
- **File → Load ROM...**: Open native file dialog
- Supports common Atari formats (.rom, .bin, .car, .atr)
- ROMs load immediately with automatic restart

### System Control
- **File → Cold Boot**: Complete system restart
- **File → Warm Boot**: Soft restart (preserves some state)
- **System → Enable BASIC**: Toggle BASIC ROM loading
- **System → Restart**: Apply BASIC setting changes

### Keyboard Input
- **Letters**: Automatically converted to uppercase (Atari standard)
- **Enter**: Execute BASIC commands or confirm actions
- **Arrows**: Navigate cursor in BASIC
- **F1-F4**: Function keys (context-dependent)
- **Ctrl+C**: Break/interrupt running programs
- **Shift+Numbers**: Access symbols `!@#$%^&*()`

## Architecture

### Components
- **AtariEmulator**: Core emulator integration and input handling
- **EmulatorWidget**: Qt widget for display and input capture
- **MainWindow**: Application window with menus and layout
- **main.cpp**: Application entry point and initialization

### Design Principles
- **Separation of Concerns**: Emulator logic separate from UI
- **Qt Best Practices**: Proper signal/slot usage, event handling
- **Performance**: Direct screen buffer access, integer scaling
- **Maintainability**: Clear class structure, documented interfaces

## Development Notes

### Key Classes
- `AtariEmulator`: Manages libatari800 lifecycle and input
- `EmulatorWidget`: Handles display rendering and keyboard events
- `MainWindow`: Provides menus and application structure

### Input Handling
- Qt key events converted to Atari key codes
- Direct mapping for letters, numbers, symbols
- Special handling for Enter, arrows, function keys
- Modifier key support (Shift, Ctrl)

### Display Pipeline
1. libatari800 generates 384x240 screen buffer
2. Display full 384x240 area without cropping
3. Convert color indices to RGB using Colours_table
4. Update QImage with converted pixels
5. Scale and display with QPainter using 98% of available space

### Timing
- QTimer drives frame updates at proper Atari speed
- Frame rate automatically detected from libatari800
- Consistent timing across different systems