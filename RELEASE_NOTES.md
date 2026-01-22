# Fujisan Release Notes

## Version 1.1.23 (Unreleased)

### New Features
- Added FujiNet and emulation speed information to status bar
- Added keyboard mappings for Insert/Delete/Clear functions:
  - Insert: Insert character (Shift+Insert: Insert line)
  - Delete: Delete character (Shift+Delete: Delete line)
  - Shift+Home: Clear screen
  - Alt/Option key variants for macOS compatibility

### Bug Fixes
- Fixed BASIC ROM configuration and checkbox state when using FujiNet
  - BASIC ROM now correctly uses user-configured ROM (not Altirra fallback) with FujiNet OS disks
  - BASIC toggle in toolbar and settings now works correctly with FujiNet enabled
  - BASIC checkbox state properly preserved when toggling FujiNet on/off
- Fixed keyboard input handling:
  - Ctrl+L now works correctly
  - Paste text now properly handles uppercase and special characters (_ $ @ # % & [ ] \ | ^)
  - Fixed case preservation during paste operations
- Fixed file permissions in Linux .deb package (binary now executable)
- Fixed Settings/Preferences dialog naming consistency across platforms

### Improvements
- Optimized Settings dialog layout for lower resolution displays (1440x900+)
  - Reduced dialog height to 730px
  - Reorganized Video/Display tab controls into horizontal rows
- Improved tooltips for BASIC behavior with FujiNet

### Platform Notes
- macOS: Dialog labeled "Preferences" with "Preference Profiles"
- Windows/Linux: Dialog labeled "Settings" with "Settings Profiles"

---

## Previous Versions

### Version 1.1.22
- Fixed Linux focus loss issues
- Fixed macOS notarization and signing
- Fixed FujiNet SD path default handling
- Disabled keyboard joysticks by default
