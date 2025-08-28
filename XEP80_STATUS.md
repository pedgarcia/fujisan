# XEP80 80-Column Display Status

## Current Implementation (v1.0.1)

### What Works
- XEP80 hardware emulation is functional
- XEP80 can be enabled in Settings > Hardware > 80-Column Cards
- Joystick port serial communication works correctly
- XEP80-aware software will detect the XEP80 hardware
- Internal fonts are used (no external charset ROM required)

### What Doesn't Work Yet
- **XEP80 display output is not visible**
- The 80-column text/graphics are generated but not shown on screen
- The "Enable 80-Column Display" option in Video Display settings is non-functional

### Why This Happens
The XEP80 generates its own separate display (like having a second monitor). While the hardware emulation works and software can communicate with it, Fujisan doesn't yet have a display widget to show the XEP80's output. This requires significant UI work to create a second display area.

### Workaround
Currently, there is no workaround to see the XEP80 display. However:
- XEP80-aware software will run correctly (just without visible 80-column output)
- The hardware emulation allows testing XEP80 compatibility
- Future versions will implement the display widget

### Future Implementation
See `TODO_XEP80.md` for detailed implementation plans. The feature requires:
1. Accessing XEP80 screen buffers from libatari800
2. Creating a separate Qt display widget for 80-column output
3. Switching between normal and XEP80 displays

### For Users
- You can enable XEP80 in settings, but you won't see the 80-column display
- This is documented in the UI tooltips
- Full XEP80 display support is planned for a future release