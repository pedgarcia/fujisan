# Fujisan Release Notes

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
