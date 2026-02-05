# Fujisan Release Notes

## Version 1.1.4 (Unreleased)

### New Features
- Added FujiNet and emulation speed information to status bar
- Added keyboard mappings for Insert/Delete/Clear functions:
  - Insert: Insert character (Shift+Insert: Insert line)
  - Delete: Delete character (Shift+Delete: Delete line)
  - Shift+Home: Clear screen
  - Alt/Option key variants for macOS compatibility
- FujiNet SD card folder moved to platform-specific user directories:
  - Automatic migration dialog with user choice options
  - "Show in Finder/Explorer" button for quick access to SD folder
- Added support to configure the H1-H4 drives via TCP API 
  - Aims to facilitate the configuration when using VSCode fastbasic-debugger 

### Bug Fixes
- Make Integer Scaling Default for crispty graphics
- Fixed layout problems with UI elements cut-off in the Media configuration screen
- Fix crash on Kubuntu 25.10
- Add windows back as "all" in build.sh
- Fixed BASIC ROM configuration and checkbox state when using FujiNet
  - BASIC ROM now correctly uses user-configured ROM (not Altirra fallback) with FujiNet OS disks
  - BASIC toggle in toolbar and settings now works correctly with FujiNet enabled
  - BASIC checkbox state properly preserved when toggling FujiNet on/off
- Fixed keyboard input handling:
  - Shift+Backspace now correctly maps to Delete Line on macOS
  - Ctrl+L now works correctly
  - Paste text now properly handles uppercase and special characters (_ $ @ # % & [ ] \ | ^)
  - Fixed case preservation during paste operations
- Fixed file permissions in Linux .deb package (binary now executable)
- Fixed Settings/Preferences dialog naming consistency across platforms
- Windows: Fixed build compilation issues

### Improvements
- Optimized Settings dialog layout for lower resolution displays (1440x900+)
  - Reduced dialog height to 730px
  - Reorganized Video/Display tab controls into horizontal rows
- Improved tooltips for BASIC behavior with FujiNet

### Platform Notes
- macOS: Dialog labeled "Preferences" with "Preference Profiles"
- Windows/Linux: Dialog labeled "Settings" with "Settings Profiles"
- Windows: NetSIO features not supported
- FujiNet SD card folder locations:
  - macOS: ~/Library/Application Support/Fujisan/
  - Windows: %APPDATA%\Fujisan\
  - Linux: ~/.local/share/Fujisan/

---

## Previous Versions

### Version 1.1.22
- Fixed Linux focus loss issues
- Fixed macOS notarization and signing
- Fixed FujiNet SD path default handling
- Disabled keyboard joysticks by default
