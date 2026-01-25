# Fujisan - Fujinet-ready Atari Emulator

![Fujisan Logo](images/fujisanlogo.png)

**Motivation**

Initially built as a modern frontend for the Atari800 emulator, Fujisan has pivoted to be a Fujinet-first Atari 8-bit emulator.

Users have plenty of good options with Altirra, Atari800 (vanilla), Atari800MacX and Mame. One thing that Atari800 has that is great, but it throws some users off, is its built-in UI inside the emulator window. A lot of users prefer to have the native feeling like Altirra on Windows, and Atari800MacX on macOS delivers. 

Fujisan is an exercise and hobby for me, to build an emulator tailored to my personal use, with fewer customization available but ready to go for 90% of the use cases. Hopefully it will attract more people like me. Also, the fact that the users will have the same native experience in MacOS or Linux.

Another important objective is to always use **libatari800** so there is never incompatibility between the Atari800 source code and Fujisan (as much as possible). There are some patches I had to apply, but I am set to always make it easier for anyone that wants to build Fujisan, to be able to reproduce my steps and patch atari800 properly (There is a patches folder with detailed instructions).

I guess, the big difference is that Fujisan is a UI-based emulator that is available for ~Windows~, Mac, and Linux. Overtime, with some features that will help software development, so if you see something wrong or have a request, use Github issue tracker.

*IMPORTANT*: For the time-being, I am dropping the Windows support. The fact that Atari800 doesn't support Fujinet on Windows makes it pointless to make efforts to run Fujisan on Windows. Users have great support with Altirra, anyways. I will keep Windows instructions available for people that might want to build and try it on that platform.


![Fujisan Screenshot](images/screenshot1.png)

## Features

### Emulator Integration
- **libatari800 Integration**: Uses the same proven emulator core
- **Authentic Display**: 384x240 full screen resolution with proper Atari colors
- **Pixel Perfect Scaling**: Integer scaling for crisp, retro graphics
- **Real-time Performance**: Proper 49.86 FPS (PAL) / 59.92 FPS (NTSC) timing
- **Fujinet-first**: Fujisan has deep integration with Fujinet PC. It comes bundle with it so you don't even have to run fujinet-pc separatelly - It also let you use Fujisan's disk and printer UI to handle fujinet media!

### Built-in FujiNet-PC
- **Bundled Binary**: FujiNet-PC comes pre-installed with Fujisan - no separate installation needed
- **Multi-Platform**: Automatically bundled for macOS (ARM64/x86_64) and Linux (x86_64/ARM64)
- **Automatic Launch**: Starts automatically when NetSIO is enabled in Settings
- **Automatic Management**: Health monitoring with auto-restart on crash
- **8 Network Drives**: D1-D8 with real-time status updates and activity LEDs
- **Smart File Handling**: Automatic copy of local disk images to FujiNet SD folder
- **Dual Drive Modes**: Seamless switching between LOCAL (Atari800) and FUJINET modes
- **Process Control**: Start, stop, restart via Settings → FujiNet dialog
- **Zero Setup**: No configuration needed - just enable NetSIO and go

### User Interface
- **Native Menus**: Standard desktop menu bar and dialogs
- **File Management**: Native file dialogs for ROM loading
- **Status Bar**: Real-time feedback for user actions
- **Focus Management**: Click-to-focus emulator display

### Developer Friendly

- Built-in TCP Server API - Fujisan includes a TCP server for remote control and automation, enabling IDE integration, automated testing, and programmatic control of all emulator features. See usage below for more details

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

### Network Features
- **Bundled FujiNet-PC**: Pre-installed FujiNet-PC binary for macOS and Linux, no separate installation required
- **Automatic Launch**: FujiNet-PC starts automatically when NetSIO is enabled (configurable)
- **Process Management**: Built-in health monitoring, auto-restart on crashes, process control in Settings
- **Smart Configuration**: Auto-configure BASIC disable and drive priorities for FujiNet boot
- **8 Network Drives**: Full D1-D8 support with real-time mount status from FujiNet web API
- **Drive Activity LEDs**: Visual read/write indicators driven by FujiNet-PC process logs
- **File Management**: Drag-and-drop disk images with automatic copy to FujiNet SD folder
- **Dual Drive Modes**:
  - **LOCAL**: Uses Atari800 core disk emulation
  - **FUJINET**: Connects to FujiNet-PC network drives
- **HTTP API Integration**: Communicates via FujiNet-PC HTTP server (default port 8000)
- **Connection Monitoring**: Continuous health checks with automatic reconnection
- **Cross-Platform**: Works seamlessly on macOS (ARM64, x86_64) and Linux (x86_64, ARM64)

### Printer Support
- **FujiNet-PC Integration**: Full printer emulation via bundled FujiNet-PC
- **17 Printer Models**: Atari 820/822/825/1020/1025/1027/1029, Epson, and more
- **Visual Output**: Animated printer form display with continuous feed simulation
- **Output Formats**: PNG, PDF, ASCII text, and raw binary
- **Print Management**: View, save, and clear printer output with tear-off animation

### Debugging & Development
- **Integrated Debugger**: 6502 debugging with breakpoints, stepping, and memory inspection
- **Breakpoint System**: Set/remove breakpoints with automatic execution pause and visual indicators
- **CPU State Monitoring**: Real-time register display (A, X, Y, PC, SP, P) in hex format
- **Memory Viewer**: Hex dump with ASCII display for full 64KB address space analysis
- **Disassembly Engine**: Full 6502 instruction set with proper mnemonics and addressing modes
- **Execution Control**: Step Into (F11), Step Over (F10), Run (F5), and Pause capabilities
- **TCP Server API**: Remote control via JSON commands for IDE integration and automated testing
- **Multi-client Support**: Event broadcasting and simultaneous connections for development workflows

### Technical Features
- **Color Accuracy**: Uses actual Atari color table from libatari800
- **Memory Efficiency**: Direct screen buffer access
- **Cross-platform**: Qt5 ensures compatibility across desktop platforms
- **Clean Architecture**: Separated emulator core, display widget, and UI

## Building

For complete build instructions, see **[BUILD.md](BUILD.md)**.

### Quick Start
```bash
# Build for macOS (both architectures)
./build.sh macos

# Build for all platforms
./build.sh all
```

### Prerequisites
- **Qt5** (Core, Widgets, Gui modules)
- **CMake 3.16+**
- **C++17 compatible compiler**
- **autoconf** and **automake** (for building atari800)

### Platform-Specific Setup

#### **macOS**
```bash
# Install Qt5 via Homebrew (recommended)
brew install qt@5

# Install CMake if not available
brew install cmake

# Install autotools for building atari800
brew install autoconf automake

# For Intel Macs, also install x86_64 version for universal builds
arch -x86_64 /usr/local/bin/brew install qt@5

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

# Install autotools for building atari800
sudo apt install autoconf automake
```

#### **Linux (CentOS/RHEL/Fedora)**
```bash
# For CentOS/RHEL
sudo yum install qt5-qtbase-devel cmake gcc-c++

# For Fedora
sudo dnf install qt5-qtbase-devel cmake gcc-c++
```

#### **Windows (supported removed for now) **
1. **Install Qt5**: Download from [qt.io](https://www.qt.io/download-qt-installer)
   - Choose Qt 5.15.x or later
   - Select "MSVC 2019 64-bit" or "MinGW" kit
2. **Install CMake**: Download from [cmake.org](https://cmake.org/download/)
3. **Install Visual Studio 2019+** (for MSVC) or **MinGW** (for GCC)

### Quick Build with Unified Script

The easiest way to build Fujisan:

```bash
# Development build (ad-hoc signing)
./build.sh macos

# Distribution build (Developer ID signing)
./build.sh macos --sign

# Cross-platform build
./build.sh all

# Build FujiNet-PC from source, then Fujisan
./build.sh macos --build-fujinet-pc

# Build everything with signing and notarization
./build.sh all --build-fujinet-pc --sign --notarize --version v1.2.0
```

#### Using Pre-built FujiNet-PC Binaries (Default)

Fujisan comes with pre-downloaded FujiNet-PC binaries. To use the latest nightly builds:

```bash
# Download latest FujiNet-PC binaries from GitHub
./scripts/download-fujinet-pc.sh

# Then build Fujisan (binaries are automatically bundled)
./build.sh macos
```

#### Building FujiNet-PC from Source (Optional)

If you want to build FujiNet-PC from source:

**Requirements:**
- Clone FujiNet-PC repository (one level above Fujisan):
  ```bash
  cd /Users/pgarcia/dev/atari/
  git clone https://github.com/FujiNetWIFI/fujinet-firmware.git
  ```
- Or specify custom location with `FUJINET_SOURCE_DIR` environment variable

**Build options:**

```bash
# Build FujiNet-PC and Fujisan for macOS (both architectures)
./build.sh macos --build-fujinet-pc

# Build FujiNet-PC and Fujisan for all platforms
./build.sh all --build-fujinet-pc

# Build with Developer ID signing and notarization
./build.sh macos --build-fujinet-pc --sign --notarize --version v1.2.0

# Custom FujiNet-PC source location
FUJINET_SOURCE_DIR=/path/to/fujinet-firmware ./build.sh macos --build-fujinet-pc
```

**What happens:**
1. FujiNet-PC is built for the requested platform(s)
2. Binaries are copied to `fujisan/fujinet/<platform>/`
3. Configuration and data files are copied alongside
4. Fujisan is then built with bundled FujiNet-PC binaries

For detailed FujiNet-PC build information, see **[docs_local/BUILD_FUJINET_PC.md](docs_local/BUILD_FUJINET_PC.md)**.

### Manual Build Steps

If you prefer to build manually or need to customize the process:

#### **Step 1: Set Environment Variable (Optional)**
```bash
export ATARI800_SRC_PATH=/path/to/atari800-src
```
**Note**: If not set, CMake will automatically download and build atari800 source.

#### **Step 2: Build Fujisan**

**Important**: We recommend out-of-source builds to keep the source directory clean. If you previously built in-source, clean up first:
```bash
# Clean up any previous in-source build files
cd /path/to/fujisan
rm -rf CMakeCache.txt CMakeFiles/ Makefile cmake_install.cmake Fujisan_autogen/
```

**macOS/Linux:**
```bash
# Navigate to Fujisan directory
cd /path/to/fujisan

# Create and enter build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..
# OR specify Qt5 path explicitly:
# CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake ..

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

#### **"configure: No such file" Error**
```
bash: ./configure: No such file or directory
```
**Solution**: Generate the configure script first:
```bash
cd $ATARI800_SRC_PATH
./autogen.sh
./configure --target=libatari800
```

#### **"ATARI800_SRC_PATH environment variable not set" Error**
```
CMake Error at CMakeLists.txt:19 (message):
  ATARI800_SRC_PATH environment variable not set.
```
**Solution**: Set the environment variable when running cmake:
```bash
ATARI800_SRC_PATH=/path/to/atari800-src cmake .
# For macOS with Homebrew Qt5:
ATARI800_SRC_PATH=/path/to/atari800-src CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake .
```

#### **"unexpected end of file in patch" Error**
```
patch: **** unexpected end of file in patch
```
**Solution**: This indicates corrupted patch files. Pull the latest fixes:
```bash
cd fujisan
git pull
```

#### **Missing Build Tools**
- **macOS**: Install Xcode Command Line Tools: `xcode-select --install`
- **Linux**: Install build-essential: `sudo apt install build-essential`
- **Windows**: Install Visual Studio with C++ tools

### Cleaning Build Files

If you need to clean your build and start fresh:

#### **Out-of-source build (recommended)**
```bash
# Simply remove the build directory
rm -rf build/
```

#### **In-source build cleanup**
```bash
# Remove all CMake-generated files
rm -rf CMakeCache.txt CMakeFiles/ Makefile cmake_install.cmake Fujisan_autogen/
# Remove compiled binary
rm -f Fujisan fujisan Fujisan.exe
```

### Code Signing and Distribution (macOS)

For distributing macOS applications, you need to sign them with a Developer ID certificate:

#### **Prerequisites for Distribution**
1. **Apple Developer Account** with Developer ID Application certificate
2. **Certificate installed** in Keychain Access
3. **App-specific password** for notarization

#### **Building for Distribution**
```bash
# Build and sign with Developer ID
./build.sh macos --sign

# This creates signed DMGs ready for notarization:
# dist/macos/Fujisan-{version}-arm64.dmg
# dist/macos/Fujisan-{version}-x86_64.dmg
```

#### **Notarization Process**
```bash
# Set up notarization credentials (one-time setup)
xcrun notarytool store-credentials "fujisan-notarization" \
    --apple-id "your-apple-id@example.com" \
    --team-id "YOUR_TEAM_ID" \
    --password "your-app-specific-password"

# Notarize the signed DMGs
./scripts/sign-and-notarize-dmgs.sh --skip-signing
```

**Note**: For development and testing, use `./build.sh macos` without `--sign` for ad-hoc signing.

### FujiNet-PC Binary Management

Fujisan bundles FujiNet-PC binaries for automatic network gaming and disk access. You have three options:

#### **Option 1: Use Pre-downloaded Binaries (Default & Recommended)**

Fujisan comes with pre-downloaded FujiNet-PC binaries in the `fujinet/` directory. To update to latest nightly builds:

```bash
./scripts/download-fujinet-pc.sh
```

This downloads the latest FujiNet-PC nightly releases for all platforms and organizes them by platform subdirectories.

#### **Option 2: Build FujiNet-PC from Source**

If you need a custom build or want to build from source:

```bash
# Clone the FujiNet-PC source repository
cd /Users/pgarcia/dev/atari/
git clone https://github.com/FujiNetWIFI/fujinet-firmware.git

# Build FujiNet-PC and Fujisan together
cd fujisan
./build.sh macos --build-fujinet-pc
```

The build script will:
- Build FujiNet-PC for your platform
- Copy binaries to `fujisan/fujinet/<platform>/`
- Build Fujisan with bundled binaries

#### **Option 3: Skip FujiNet-PC**

If you don't need FujiNet support, you can build without bundled binaries:

```bash
./build.sh macos --no-fujinet
```

#### **FujiNet-PC Documentation**

For comprehensive information about:
- Binary management and versions
- Building FujiNet-PC from source
- Technical protocol details
- Troubleshooting

See: **[docs_local/FUJINET_PC.md](docs_local/FUJINET_PC.md)** and **[docs_local/BUILD_FUJINET_PC.md](docs_local/BUILD_FUJINET_PC.md)**

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

### FujiNet-PC Usage

#### Getting Started
1. **Enable NetSIO**: Settings → Hardware → NetSIO checkbox
2. **Automatic Setup**:
   - FujiNet-PC launches automatically (if auto-launch enabled)
   - BASIC is auto-disabled (required for FujiNet boot)
   - Drives switch to FUJINET mode
   - Connection health monitoring starts
3. **Use Network Drives**:
   - Access online services, file sharing, multiplayer games
   - D1-D8 network drives available
   - Real-time status updates in drive widgets

#### Managing Drives
- **Insert Disk**: Click drive widget, select .atr/.xfd/.dcm file
  - Files automatically copied to FujiNet SD folder (with progress indicator)
  - Mounted to selected drive after successful copy
  - Activity LEDs show read/write operations
- **Eject Disk**: Right-click drive widget → Eject
- **Drive Status**: Blue background indicates FUJINET mode
- **D3-D8 Access**: Click drawer icon to expand additional drives

#### Printer Integration
- **Enable Printer**: Settings → FujiNet → Enable Printer checkbox
- **Select Model**: Choose from 17 printer types (Atari 820-1029, Epson, etc.)
- **View Output**: Printer icon appears in toolbar when output ready
  - Click to view animated printer form
  - Save as PNG/PDF or view ASCII text
  - Tear-off animation when clearing output

#### Process Control
- **Settings → FujiNet Tab**:
  - View FujiNet-PC status (running/stopped)
  - Start/Stop/Restart buttons
  - View process output (stdout/stderr)
  - Configure SD folder path
  - Set HTTP API port (default: 8000)
  - Set NetSIO UDP port (default: 9997)
  - Choose launch behavior: Auto/Detect Existing/Manual

#### Connection Status
- **Status Bar Indicators**:
  - "Drives: FujiNet (NetSIO enabled)" - Connected and ready
  - "FujiNet Disconnected" (with spinner) - Attempting reconnection
  - "Copying disk.atr to FujiNet... 45%" - File transfer progress
  - Drive mount success/failure messages

#### Requirements
- **Ports**: HTTP API on 8000, NetSIO UDP on 9997 (configurable)
- **SD Folder**: Defaults to `<fujinet-binary-path>/SD/`
- **Network**: Localhost communication between Fujisan and FujiNet-PC

### Keyboard Input
- **Letters**: Automatically converted to uppercase (Atari standard)
- **Enter**: Execute BASIC commands or confirm actions
- **Arrows**: Navigate cursor in BASIC
- **F1-F4**: Function keys (context-dependent)
- **Ctrl+C**: Break/interrupt running programs
- **Shift+Numbers**: Access symbols `!@#$%^&*()`

### Known Limitations
- **Local Printer (P: device)**: Direct P: device emulation is disabled due to Error 138 (Device Timeout) issues in the atari800 core. Use FujiNet-PC printer integration instead for full printer functionality.

## Configuration Files

Fujisan stores its configuration in platform-specific locations following standard OS conventions.

### Main Settings

Application settings (speed, audio, video, etc.) are stored using Qt's native settings format:

**macOS:**
```
~/Library/Preferences/com.8bitrelics.Fujisan.plist
```

**Windows:**
```
Registry: HKEY_CURRENT_USER\Software\8bitrelics\Fujisan
```

**Linux:**
```
~/.config/8bitrelics/Fujisan.conf
```

### Configuration Profiles

Machine configuration profiles (complete emulator setups) are stored as JSON files:

**macOS:**
```
~/Library/Application Support/Fujisan/profiles/*.profile
```

**Windows:**
```
C:\Users\<username>\AppData\Roaming\Fujisan\profiles\*.profile
```

**Linux:**
```
~/.local/share/Fujisan/profiles/*.profile
```

Profiles contain complete machine configurations including hardware settings, memory configuration, peripherals, and media paths. They can be managed through the Settings dialog's profile system.

### FujiNet-PC Settings

FujiNet-PC configuration and process control available in Settings dialog (FujiNet tab).

**Binary Configuration:**
- Binary path (bundled with Fujisan, platform-specific)
- Version detection and display
- Platform: macOS (ARM64/x86_64), Linux (x64/ARM64)

**SD Card Settings:**
- SD folder path (default: `<binary-path>/SD/`)
- Custom path configuration for virtual SD card storage

**Server Configuration:**
- HTTP API port (default: 8000, used for drive/printer communication)
- NetSIO UDP port (default: 9997, used by Atari800 core)
- Launch behavior:
  - **Auto Launch**: Start FujiNet-PC when NetSIO enabled
  - **Detect Existing**: Connect to already-running FujiNet-PC
  - **Manual**: User controls start/stop

**Process Management:**
- Real-time status display (running/stopped)
- Start/Stop/Restart buttons
- Process output viewer (stdout/stderr with scrollback)
- Auto-restart on NetSIO enable (if auto-launch mode)

**Configuration File:**
- Location: `<binary-path>/fnconfig.ini`
- Auto-updated with NetSIO port before launch
- Contains printer, network, and device settings

## TCP Server API

Fujisan includes a TCP server for remote control and automation, enabling IDE integration, automated testing, and programmatic control of all emulator features.

### Enabling the Server
1. Go to **Tools → TCP Server** in the menu
2. Server starts on `localhost:8080`
3. Multiple clients can connect simultaneously

### Key Features
- **JSON Protocol**: Simple command/response format
- **Complete Control**: Media, system, input, debug, and configuration commands
- **Event Broadcasting**: Real-time notifications to all connected clients
- **Security**: Localhost-only binding for safety

### Quick Example
```bash
# Get emulator state
echo '{"command": "status.get_state"}' | nc localhost 8080

# Load a disk
echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/path/to/disk.atr"}}' | nc localhost 8080

# Send text to emulator
echo '{"command": "input.send_text", "params": {"text": "LOAD \"D:*\"\n"}}' | nc localhost 8080
```

### Complete Documentation
See **[TCP_SERVER_API.md](TCP_SERVER_API.md)** for complete API documentation with examples for all 34+ available commands covering media control, debugging, configuration, and automation.

## Debugging

Fujisan includes an integrated debugger for Atari 8-bit programs. Access it via **Tools → Debug Window** in the menu bar.

### Debugger Features

#### **CPU State Monitoring**
- **Registers**: Real-time display of A, X, Y, PC, SP, and P registers
- **Hex Format**: All values shown in standard 6502 hex notation ($xx/$xxxx)
- **Live Updates**: Registers update automatically during emulation

#### **Execution Control**
- **Step Into** (`F11`): Execute one instruction at current PC
- **Step Over** (`F10`): Step over subroutine calls (JSR instructions)
- **Run** (`F5`): Continue execution from current state
- **Pause**: Stop execution and examine current state

#### **Breakpoint System**
- **Set Breakpoints**: Enter any address ($0000-$FFFF) and click "Add"
- **Visual Indicators**: Breakpoints marked with `B` in disassembly view
- **Automatic Pause**: Execution stops when PC reaches breakpoint address
- **Management**: Add, remove, or clear all breakpoints
- **Persistence**: Breakpoints saved between sessions
- **Keyboard Shortcut**: `Ctrl+B` to add breakpoint at current address

#### **Disassembly View**
- **Current Instruction**: Highlighted display of instruction at PC
- **Instruction Window**: Scrollable view of surrounding code
- **Proper Mnemonics**: Full 6502 instruction set with operands
- **Address Modes**: Correct display of immediate (#$xx), absolute ($xxxx), indexed, etc.
- **Visual Markers**: 
  - `->` marks current PC location
  - `B` marks breakpoint addresses
- **Auto-Centering**: Current PC automatically centered when paused

#### **Memory Viewer**
- **Hex Dump**: Traditional hex editor style display
- **ASCII Column**: Printable characters shown alongside hex values
- **Address Navigation**: Jump to any memory location ($0000-$FFFF)
- **Live Display**: Memory contents update in real-time
- **Scrollable**: View any portion of 64KB address space

### Debugging Workflow

#### **Setting Up Debug Session**
1. **Open Debugger**: Tools → Debug Window
2. **Set Breakpoints**: Enter addresses where you want execution to pause
3. **Start Program**: Load ROM/disk and run normally
4. **Pause Execution**: Use Pause button or wait for breakpoint

#### **Typical Debug Workflow**
```
1. Set breakpoint at program entry point (e.g., $2000)
2. Press Run to start execution
3. Emulator pauses at breakpoint automatically
4. Examine CPU registers and memory state
5. Step through code with F11 (Step Into) or F10 (Step Over)
6. Set additional breakpoints as needed
7. Continue with F5 (Run) to next breakpoint
```

#### **Breakpoint Management**
- **Add**: Enter address and click "Add" or press `Ctrl+B`
- **Remove**: Select breakpoint in list and click "Remove"
- **Clear All**: Remove all breakpoints at once
- **Visual**: Breakpoints shown with `B` marker in disassembly

#### **Memory Analysis**
- **Jump to Address**: Change memory viewer address to examine specific locations
- **Watch Variables**: Monitor memory locations for data changes
- **Stack Analysis**: Check stack pointer (SP) and stack contents
- **Zero Page**: Examine zero page variables ($00-$FF)

#### **Code Analysis**
- **Subroutine Calls**: Use Step Over (F10) to skip JSR instructions
- **Instruction Flow**: Use Step Into (F11) to trace exact execution path
- **Branch Analysis**: See branch targets and conditional execution
- **Address Modes**: Understand how instructions access memory

### Debug Information Display

#### **CPU Registers Format**
```
A: $FF  X: $00  Y: $00
PC: $2000  SP: $FF  P: $34
```

#### **Disassembly Format**
```
   $1FFE: 20 00 20  JSR $2000
B  $2001: A9 FF     LDA #$FF      <- Breakpoint
-> $2003: 85 10     STA $10       <- Current PC
   $2005: 60        RTS
```

#### **Memory Dump Format**
```
0800: 20 00 E4 20 5C E4 A2 00 A0 02 20 5A E4 06 07 A5 |  .ä \ä¢ ¥ Zä..¥
0810: 07 D0 FB A6 07 A4 08 20 A5 E4 A9 9B 20 D2 FF A9 |.ÐûÞ¤. ¥ä© ÒÿÞ
```

### Debugging Tips

#### **Common Breakpoint Locations**
- **Program Start**: Main program entry point
- **Interrupt Vectors**: IRQ/NMI handlers ($FFFE, $FFFA)
- **System Calls**: OS ROM routines
- **Critical Loops**: Main game/program loops
- **Error Conditions**: Error handling code

#### **Performance Considerations**
- **Minimize Breakpoints**: Too many can slow execution
- **Use Step Over**: For subroutines you don't need to trace
- **Pause When Needed**: Continuous debugging updates use CPU cycles

#### **Memory Regions of Interest**
- **Zero Page** ($00-$FF): Fast access variables
- **Page 2** ($200-$2FF): System stack
- **Page 6** ($600-$6FF): Common program area
- **System Variables** ($00-$7F): OS variables

### Advanced Debugging

#### **Instruction Set Knowledge**
Understanding 6502 assembly helps with debugging:
- **Branch Instructions**: BEQ, BNE, BCC, BCS affect program flow
- **Subroutine Calls**: JSR pushes return address to stack
- **Stack Operations**: PHA/PLA, PHP/PLP for register save/restore
- **Addressing Modes**: Zero page vs absolute addressing

#### **Atari-Specific Debugging**
- **ANTIC/GTIA**: Graphics chips affect display
- **POKEY**: Sound and I/O operations
- **PIA**: Joystick and keyboard input
- **Device Handlers**: Disk, cassette, printer operations

The debugger offers extensive capabilities for understanding and debugging Atari 8-bit programs.

## Architecture

### Components
- **AtariEmulator**: Core emulator integration and input handling
- **EmulatorWidget**: Qt widget for display and input capture
- **MainWindow**: Application window with menus and layout
- **DebuggerWidget**: Debugging interface with breakpoints and execution control
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
- `DebuggerWidget`: Provides debugging capabilities with breakpoint management and 6502 analysis

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
