# Fujisan Release Notes

## Version 1.1.5

### New Features

- **Keyboard joystick presets:** Joystick keyboard emulation now lets you select presets to customize which keys are used for each joystick, with built-in support for NumPad, Arrow keys, and WASD layouts.
- **Linux FujiNet-PC updated:** Updated fujinet-pc with the latest changes, including important fixes while handling JSON data.

### Bug Fixes

- **Tab key captured by Fujisan instead of emulator:** Tab (and Shift+Tab) keypresses were being intercepted by Fujisan's UI focus system and never sent to the emulated Atari. This prevented using Tab for navigation inside Atari programs — most notably the FujiNet configuration menu. In some cases the stray focus changes inadvertently altered Fujisan's own settings. Fixed by intercepting the key at the Qt event level before focus navigation can grab it.
- **XEX loading via TCP API broken when FujiNet/NetSIO is active:** When FujiNet was enabled, loading XEX files through the TCP API (e.g. from the fastbasic-debugger VSCode extension) failed because D1: commands were unconditionally routed to FujiNet-PC. BINLOAD now takes priority during XEX load and hands control back to NetSIO afterwards.
- **macOS FujiNet-PC OpenSSL signing error:** Fixed a code-signing error ("Team ID mismatch") that prevented FujiNet-PC from launching on macOS when built without an Apple Developer certificate. OpenSSL dylibs are now bundled and re-signed as part of the app.

### Improvements

- XEX loading is faster when FujiNet is active — the emulator core now reads XEX data in chunks instead of byte-by-byte, reducing I/O overhead during load.

# Previous Version(s)

## Version 1.1.4

### New Features
- Updated fujinet-pc binaries to include temporary fix that prevent reading failures when reading larger JSON objects (https://github.com/FujiNetWIFI/fujinet-firmware/pull/1155). (The final fix is still being worked by the Fujinet team)
- Added support to configure the H1-H4 drives via TCP API 
  - Aims to facilitate the configuration when using VSCode fastbasic-debugger 

### Bug Fixes
- Fixed problem where the H1-H4 hard-drive configuration was been ignored when starting atari800 core
- **Linux x86:** Fixed SIGILL crash on Intel Ivy Bridge (Mac Mini 2012 running linux) and other PCs with older CPUs. Release builds now use x86-64-v2 baseline instead of native CPU; set `FUJISAN_NATIVE_CPU=1` for native optimizations on modern hardware.
- Make Integer Scaling Default for crispty graphics
- Fixed layout problems with UI elements cut-off in the Media configuration screen
- Fix crash on Kubuntu 25.10

### Improvements
- Optimized Settings dialog layout for lower resolution displays (1440x900+)
  - Reduced dialog height to 730px
  - Reorganized Video/Display tab controls into horizontal rows
