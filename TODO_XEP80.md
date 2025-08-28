# XEP80 80-Column Display Implementation TODO

## Current Status
- ✅ XEP80 hardware emulation is compiled into libatari800
- ✅ XEP80 can be enabled in Settings > Hardware > 80-Column Cards
- ✅ XEP80 joystick port serial communication works
- ❌ XEP80 display output is not visible (no display widget)
- ❌ libatari800 doesn't export XEP80 screen buffers

## Technical Background

### XEP80 Architecture
The XEP80 is an 80-column display adapter that:
- Connects to joystick port 2 (or port 1 with configuration)
- Communicates via serial protocol at 15.7 kHz
- Generates its own video signal separate from the main Atari display
- Has its own character ROM (or can use internal fonts with patches)
- Produces 640x200 (80x25 text) or 320x200 (graphics) display

### XEP80 Display Buffers
The XEP80 emulation maintains two screen buffers:
- `XEP80_screen_1[XEP80_SCRN_WIDTH * XEP80_MAX_SCRN_HEIGHT]` 
- `XEP80_screen_2[XEP80_SCRN_WIDTH * XEP80_MAX_SCRN_HEIGHT]`
- Buffer dimensions: 640x200 pixels (80 columns x 25 rows x 8 pixels)
- Alternates between buffers for smooth updates

### How Other Emulators Handle XEP80

#### Altirra (Windows)
- Creates separate `ATXEP80Emulator` class
- Maintains own framebuffer (`VDPixmap mFrame`)
- `GetFrameBuffer()` returns the XEP80 display buffer
- Has dedicated window/display area for XEP80 output

#### atari800 (SDL version)
- Uses `VIDEOMODE` system with `VIDEOMODE_MODE_XEP80`
- Switches between normal and XEP80 display modes
- `SDL_VIDEO_BlitXEP80_*` functions handle XEP80 rendering
- `-80column` command-line flag enables 80-column display mode

#### Atari800MacX
- When `PLATFORM_80col` is enabled, switches display source
- Points `screen` to `XEP80_screen_1` or `XEP80_screen_2`
- Handles different dimensions (640 vs 320 width)
- Automatic switching when XEP80 is accessed

## Implementation Requirements

### Phase 1: libatari800 API Extensions
Need to add to libatari800/api.c:
```c
// Get current XEP80 screen buffer
UBYTE* LIBATARI800_GetXEP80Screen();

// Check if XEP80 is active/enabled
int LIBATARI800_IsXEP80Active();

// Get XEP80 display dimensions
void LIBATARI800_GetXEP80Dimensions(int *width, int *height);

// Get XEP80 dirty region for optimized updates
int LIBATARI800_GetXEP80DirtyRect(int *x, int *y, int *w, int *h);
```

### Phase 2: Qt Frontend Display Widget
1. Create `XEP80DisplayWidget` class:
   - Handles 640x200 or 320x200 display
   - Converts indexed color to RGB
   - Scales appropriately for window

2. Modify `AtariDisplay` class:
   - Add XEP80 display widget alongside main display
   - Switch between displays based on XEP80 state
   - Handle different aspect ratios

3. Update `AtariEmulator::getScreen()`:
   - Check if XEP80 is active
   - Return XEP80 buffer when appropriate
   - Handle buffer dimensions change

### Phase 3: Display Switching Logic
1. Automatic switching:
   - Monitor XEP80 enable state
   - Switch display when software accesses XEP80
   - Return to normal display when XEP80 disabled

2. Manual control:
   - Add "View > 80-Column Display" menu option
   - Keyboard shortcut for quick switching
   - Remember user preference

### Phase 4: Configuration & Settings
1. Settings dialog updates:
   - XEP80 port selection (1 or 2)
   - Character ROM path (optional with internal fonts)
   - Display preferences (color, scaling)

2. Command-line arguments:
   - `-xep80` - enable XEP80
   - `-xep80port <1|2>` - select joystick port
   - `-80column` - show 80-column display

## Build System Changes

### Current Issues
1. libatari800 target excludes VIDEOMODE module
2. XEP80 requires VIDEOMODE for display switching
3. Need to export XEP80 screen buffers from library

### Solutions
1. Modify libatari800 build to include minimal XEP80 display support
2. Or: Access XEP80 buffers directly via exported symbols
3. Or: Implement display switching entirely in Qt frontend

## Testing Requirements

### Test Software
- XEP80 handler disk (official Atari software)
- 80-column word processors (AtariWriter 80, etc.)
- Terminal programs with 80-column support
- Graphics mode test programs

### Test Scenarios
1. Enable XEP80, verify handler loads
2. Switch between 40/80 column modes
3. Test text output in 80-column mode
4. Test graphics mode (320x200)
5. Test with different machine types
6. Verify joystick port 1 vs 2 configuration

## Known Limitations

### Current Implementation
- XEP80 hardware emulation works but display not visible
- No way to see 80-column output
- Software thinks XEP80 is present but user sees nothing

### After Implementation
- Will need separate window/widget for XEP80
- Performance impact of dual displays
- Synchronization between main and XEP80 displays

## References
- XEP80 Technical Manual
- AtariAge XEP80 discussion: http://www.atariage.com/forums/topic/108601-what-does-xep80-display-look-like/
- Original XEP80 handler source code
- Altirra XEP80 implementation: src/Altirra/source/xep80.cpp
- atari800 XEP80: src/xep80.c, src/videomode.c
- Atari800MacX: DisplayManager XEP80 handling

## Priority
**LOW** - While XEP80 is a nice feature, it's used by relatively few programs. The main emulation works fine without it. Focus on core functionality first.

## Temporary Workaround
Until full implementation:
1. Keep XEP80 option in settings
2. Add tooltip/help text: "XEP80 hardware emulation enabled (display output not yet visible)"
3. Document in release notes that XEP80 hardware works but display not implemented
4. This allows XEP80-aware software to detect and use XEP80 even if output isn't visible