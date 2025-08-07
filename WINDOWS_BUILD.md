# Windows Build Documentation for Fujisan

This document provides complete instructions for building Fujisan for Windows using cross-compilation on macOS/Linux.

## Prerequisites

### Required Tools
- **Podman or Docker** - Container runtime
- **Git** - Version control
- **macOS or Linux host** - Cross-compilation environment

### Container Image
We use the `maxrd2/arch-mingw` container which provides:
- MinGW-w64 cross-compilation toolchain
- x86_64-w64-mingw32 target
- Essential build tools (cmake, make, autotools)
- Qt5 libraries for Windows

## Build Process Overview

### 1. Project Setup
```bash
git clone https://github.com/pedgarcia/fujisan.git
cd fujisan
```

### 2. Container-based Cross-compilation

#### Basic Windows Build (Working)
```bash
# Create build directory  
mkdir -p build-cross-windows
cd build-cross-windows

# Configure with CMake in container
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    cmake \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    ..

# Build Fujisan
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    make -j4
```

#### With FujiNet Support (Experimental)
```bash
# Enable FujiNet/NetSIO support (currently limited on Windows)
podman run --rm -v "$(pwd)/../:/work" -w /work/build-cross-windows maxrd2/arch-mingw \
    cmake \
    -DENABLE_FUJINET=ON \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    ..
```

## Build Scripts

The project includes several build scripts for different scenarios:

### 1. `scripts/build-libatari800-simple.sh`
- **Purpose**: Basic Windows build verification
- **Used by**: Standard Windows builds
- **Function**: Checks if libatari800.a already exists

### 2. `scripts/build-libatari800-fujinet.sh`
- **Purpose**: Builds libatari800 with FujiNet/NetSIO support
- **Used by**: When `-DENABLE_FUJINET=ON` is specified
- **Function**: Configures with `--enable-netsio --enable-rnetwork`

### 3. `scripts/create-minimal-makefile.sh`
- **Purpose**: Fallback minimal build system
- **Used by**: Legacy Windows builds (MSYS2)
- **Function**: Creates minimal Makefile when autotools fails

## File Structure After Build

```
build-cross-windows/
├── Fujisan.exe                    # Main executable (4.98MB)
├── atari800-src/                  # Downloaded atari800 source
│   ├── src/libatari800.a          # Built library (~978KB)
│   ├── src/config.h               # Build configuration
│   └── fujisan-patches/           # Applied patches
├── windows-release/               # Deployment package
│   ├── Fujisan.exe
│   ├── *.dll                      # 21 runtime dependencies
│   ├── platforms/qwindows.dll     # Qt platform plugin
│   ├── images/                    # UI graphics
│   └── README.txt                 # Deployment instructions
```

## Deployment Package Creation

### 1. Copy Main Executable
```bash
cp build-cross-windows/Fujisan.exe windows-release/
```

### 2. Extract Required DLLs from Container
```bash
# Qt5 libraries
podman run --rm -v "$(pwd):/work" maxrd2/arch-mingw \
    cp /usr/x86_64-w64-mingw32/lib/libQt5*.dll /work/windows-release/

# MinGW runtime libraries
podman run --rm -v "$(pwd):/work" maxrd2/arch-mingw \
    cp /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll \
       /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll \
       /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll \
       /work/windows-release/

# Additional system libraries (discovered through testing)
podman run --rm -v "$(pwd):/work" maxrd2/arch-mingw \
    cp /usr/x86_64-w64-mingw32/bin/libiconv-2.dll \
       /usr/x86_64-w64-mingw32/bin/libpcre2-16-0.dll \
       /usr/x86_64-w64-mingw32/bin/libssp-0.dll \
       /usr/x86_64-w64-mingw32/bin/zlib1.dll \
       /usr/x86_64-w64-mingw32/bin/libharfbuzz-0.dll \
       /usr/x86_64-w64-mingw32/bin/libpng16-16.dll \
       /usr/x86_64-w64-mingw32/bin/libfreetype-6.dll \
       /usr/x86_64-w64-mingw32/bin/libglib-2.0-0.dll \
       /usr/x86_64-w64-mingw32/bin/libgraphite2.dll \
       /usr/x86_64-w64-mingw32/bin/libpcre-1.dll \
       /usr/x86_64-w64-mingw32/bin/libintl-8.dll \
       /usr/x86_64-w64-mingw32/bin/libbz2-1.dll \
       /work/windows-release/
```

### 3. Copy Qt Platform Plugin
```bash
mkdir -p windows-release/platforms
podman run --rm -v "$(pwd):/work" maxrd2/arch-mingw \
    cp /usr/x86_64-w64-mingw32/lib/qt/plugins/platforms/qwindows.dll \
       /work/windows-release/platforms/
```

### 4. Copy UI Images
```bash
cp -r images/ windows-release/
```

## Complete Dependency List

### Runtime DLLs Required (21 total)
1. **Qt5 Libraries (5)**:
   - Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, Qt5Network.dll, Qt5Multimedia.dll

2. **MinGW Runtime (3)**:
   - libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll

3. **System Libraries (13)**:
   - libiconv-2.dll, libpcre2-16-0.dll, libssp-0.dll, zlib1.dll
   - libharfbuzz-0.dll, libpng16-16.dll, libfreetype-6.dll  
   - libglib-2.0-0.dll, libgraphite2.dll, libpcre-1.dll
   - libintl-8.dll, libbz2-1.dll

### Platform Plugin
- `platforms/qwindows.dll` - Required for Qt GUI

### UI Assets
- `images/` directory with PNG graphics for drive states and logos

## Key Technical Details

### libatari800 Integration
- **Patches Applied**: 
  - `patches/0001-libatari800-disk-api.patch` - Adds Fujisan-specific API functions
  - `patches/0002-windows-ulong-conflicts.patch` - Fixes Windows type conflicts
- **API Extensions**: Disk activity callbacks, drive management, SIO patch control
- **Build Target**: `--target=libatari800` produces static library

### Windows-Specific Fixes
1. **Type Conflicts**: Windows `ULONG` (8 bytes) vs atari800 `unsigned int` (4 bytes)
2. **Missing Symbols**: Added `ANTIC_lookup_gtia9/11` definitions 
3. **Header Issues**: `WIN32_LEAN_AND_MEAN`, `NOMINMAX` preprocessor definitions

### CMake Configuration
```cmake
# Windows cross-compilation detection
if(CMAKE_CROSSCOMPILING AND WIN32)
    # Static linking attempt (partially successful)
    set(Qt5_USE_STATIC_LIBS ON)
    target_link_libraries(${PROJECT_NAME} -static-libgcc -static-libstdc++)
    
    # Windows system libraries
    target_link_libraries(${PROJECT_NAME} 
        -lws2_32 -lkernel32 -luser32 -lgdi32 [...])
endif()
```

## Known Limitations

### FujiNet/NetSIO Support
- **Status**: UI present but functionality disabled
- **Issue**: Windows socket functions not detected by autotools configure
- **Workaround**: Requires manual config.h modification to enable NETSIO
- **Dependencies**: Needs proper Winsock2 integration

### Static Linking
- **Qt5 Libraries**: Remain dynamically linked despite static flags
- **Solution**: Complete DLL package required for deployment
- **Benefit**: Easier to deploy all dependencies together

## Testing

### Verification Steps
1. **Build Success**: Check for `Fujisan.exe` creation (~5MB)
2. **Library Verification**: Confirm `libatari800.a` exists (~978KB)
3. **Windows Testing**: Deploy to Windows VM/machine
4. **Runtime Testing**: Verify GUI loads, images display, ROMs can be configured

### Test Environment
- **Platform**: Windows ARM VM (tested)
- **Requirements**: Windows 10 or later
- **Deployment**: All files in same directory as Fujisan.exe

## Troubleshooting

### Common Issues

1. **Missing DLLs**:
   - **Symptoms**: Runtime error about missing DLL
   - **Solution**: Copy additional DLLs from container `/usr/x86_64-w64-mingw32/bin/`

2. **Qt Platform Plugin Error**:
   - **Symptoms**: "Could not find QT platform plugin 'windows'"
   - **Solution**: Ensure `platforms/qwindows.dll` exists

3. **Missing Images**:
   - **Symptoms**: UI shows placeholder graphics
   - **Solution**: Copy `images/` directory to deployment package

4. **Container Platform Warning**:
   - **Message**: "image platform (linux/amd64) does not match (linux/arm64)"
   - **Impact**: None - container works despite warning on Apple Silicon

## Future Improvements

1. **True Static Linking**: Investigate Qt5 static builds for MinGW
2. **FujiNet Integration**: Resolve Windows socket detection issues
3. **Automated DLL Collection**: Script to automatically gather all dependencies
4. **Code Signing**: Add Windows code signing for release builds
5. **Installer Package**: Create Windows installer (.msi) for easier deployment

## Version Information

- **Build Date**: August 2025
- **Container**: maxrd2/arch-mingw (Arch Linux with MinGW-w64)
- **Target**: x86_64 Windows (64-bit)
- **Qt Version**: 5.x
- **libatari800**: Latest with Fujisan API extensions