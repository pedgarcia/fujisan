Fujisan - Modern Atari Emulator for Windows
==========================================

This is a Windows build of Fujisan, a Qt5-based frontend for the Atari800 emulator.

Contents:
- Fujisan.exe        - Main emulator executable
- Qt5Core.dll        - Qt5 Core library
- Qt5Gui.dll         - Qt5 GUI library  
- Qt5Widgets.dll     - Qt5 Widgets library
- Qt5Network.dll     - Qt5 Network library (for TCP server API)
- Qt5Multimedia.dll  - Qt5 Multimedia library (for audio)
- libgcc_s_seh-1.dll - MinGW C runtime library
- libstdc++-6.dll    - MinGW C++ runtime library
- libwinpthread-1.dll - MinGW threading library
- libiconv-2.dll     - Character encoding conversion library
- libpcre2-16-0.dll  - PCRE2 regular expression library (UTF-16)
- libssp-0.dll       - Stack Smashing Protection library
- zlib1.dll          - Data compression library
- libharfbuzz-0.dll  - Text shaping engine library
- libpng16-16.dll    - PNG image format library
- libfreetype-6.dll  - Font rendering library
- libglib-2.0-0.dll  - GLib core utility library
- libgraphite2.dll   - Graphite2 smart font rendering library
- libpcre-1.dll      - PCRE regular expression library (v1)
- libintl-8.dll      - GNU internationalization library
- libbz2-1.dll       - Bzip2 compression library
- platforms/         - Qt platform plugins directory
  - qwindows.dll     - Windows platform plugin (required for GUI)
- images/            - UI graphics and logos directory
  - fujisanlogo.png  - Fujisan logo for toolbar and about dialog
  - atari810*.png    - Disk drive state graphics (off/empty/closed/read/write)
  - cassette*.png    - Cassette tape state graphics (off/on)
  - cartridge*.png   - Cartridge slot state graphics (off/on)

Requirements:
- Windows 10 or later
- All DLL files must be in the same directory as Fujisan.exe

Features:
- Full Atari 400/800/800XL/130XE/5200 emulation
- Modern Qt5 interface with authentic Atari styling  
- TCP server API on port 8080 for IDE integration
- Comprehensive debugging capabilities
- Complete media management (disks, cartridges, cassettes)
- FujiNet/NetSIO network support
- Disk activity monitoring with visual indicators

To run:
1. Ensure all files are in the same directory
2. Double-click Fujisan.exe or run from command line
3. Configure ROMs and media files through the interface

Build Information:
- Cross-compiled with MinGW-w64 on Linux
- libatari800 version: Latest (2025) with Fujisan API extensions
- Qt5 version: 5.x (dynamically linked)
- Architecture: x86_64 (64-bit)

For more information, visit: https://github.com/8bitrelics/fujisan