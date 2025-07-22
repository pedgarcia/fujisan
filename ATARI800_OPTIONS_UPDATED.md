# Fujisan - Complete Atari800 Command Line Options Reference (Updated from Manual)

This document provides a comprehensive mapping of ALL command line options and configuration settings available in the Atari800 emulator based on the official atari800man.txt manual. This reference is useful for understanding what features could be integrated into GUI frontends and for advanced configuration.

## Table of Contents

- [Machine Configuration](#machine-configuration)
- [ROM Configuration](#rom-configuration)
- [Cartridge Configuration](#cartridge-configuration)
- [Media Configuration](#media-configuration)
- [Device Configuration](#device-configuration)
- [Hardware Extensions](#hardware-extensions)
- [Performance and Timing](#performance-and-timing)
- [Audio Configuration](#audio-configuration)
- [Video and Display](#video-and-display)
- [Input Configuration](#input-configuration)
- [Event Recording/Playback](#event-recordingplayback)
- [Debugging and Development](#debugging-and-development)
- [Platform-Specific Options](#platform-specific-options)
- [Configuration File Parameters](#configuration-file-parameters)
- [GUI Implementation Status](#gui-implementation-status)
- [Usage Examples](#usage-examples)

---

## Machine Configuration

### Machine Types
| Option | Description | RAM | BASIC | Notes | GUI Status |
|--------|-------------|-----|-------|-------|------------|
| `-atari` | Atari 400/800 | 48K | No | Original model | ✅ Implemented |
| `-1200` | Atari 1200XL | 64K | No | Keyboard LEDs, F-keys | ✅ Implemented |
| `-xl` | Atari 800XL | 64K | Yes | Most common model | ✅ Implemented |
| `-xe` | Atari 130XE | 128K | Yes | Extended memory | ✅ Implemented |
| `-320xe` | Atari 320XE | 320K | Yes | Compy-Shop variant | ✅ Implemented |
| `-rambo` | Atari 320XE | 320K | Yes | Rambo XL variant | ✅ Implemented |
| `-576xe` | Atari 576XE | 576K | Yes | Large memory | ✅ Implemented |
| `-1088xe` | Atari 1088XE | 1088K | Yes | Maximum memory | ✅ Implemented |
| `-xegs` | Atari XEGS | 64K | Yes | Game system with built-in game | ✅ Implemented |
| `-5200` | Atari 5200 | 16K | No | Game console | ✅ Implemented |

### TV Mode
| Option | Description | Frequency | Scanlines | GUI Status |
|--------|-------------|-----------|-----------|------------|
| `-pal` | PAL TV mode | 50Hz | 312 | ✅ Implemented |
| `-ntsc` | NTSC TV mode | 60Hz | 262 | ✅ Implemented |

### Memory Configuration
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-c` | Enable RAM between 0xc000-0xcfff in Atari 800 | ✅ Implemented |
| `-mosaic <n>` | Mosaic memory expansion: n KB total RAM | ✅ Implemented |
| `-axlon <n>` | Axlon memory expansion: n KB total RAM | ✅ Implemented |
| `-axlon0f` | Use Axlon shadow at 0x0fc0-0x0fff | ✅ Implemented |
| `-mapram` | Enable MapRAM for XL/XE | ✅ Implemented |
| `-no-mapram` | Disable MapRAM | ✅ Implemented |
| `-ram-size <n>` | Set RAM size in KB | ❌ Missing |

### BASIC Configuration
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-basic` | Enable Atari BASIC ROM | ✅ Implemented |
| `-nobasic` | Disable Atari BASIC ROM | ✅ Implemented |

---

## ROM Configuration

### ROM Files
| Option | Description | Machine | Size | GUI Status |
|--------|-------------|---------|------|------------|
| `-osa_rom <file>` | OS A ROM | 400/800 | 10KB | ✅ Implemented |
| `-osb_rom <file>` | OS B ROM | 400/800 | 10KB | ✅ Implemented |
| `-xlxe_rom <file>` | XL/XE OS ROM | XL/XE | 16KB | ✅ Implemented |
| `-5200_rom <file>` | 5200 BIOS ROM | 5200 | 2KB | ✅ Implemented |
| `-basic_rom <file>` | BASIC ROM | All 8-bit | 8KB | ✅ Implemented |

### ROM Versions
| Option | Values | Description | GUI Status |
|--------|--------|-------------|------------|
| `-800-rev` | `auto\|a-ntsc\|a-pal\|b-ntsc\|custom\|altirra` | 400/800 OS revision | ❌ Missing |
| `-xl-rev` | `auto\|10\|11\|1\|2\|3a\|3b\|5\|3\|4\|59\|59a\|custom\|altirra` | XL/XE OS revision | ❌ Missing |
| `-5200-rev` | `auto\|orig\|a\|custom\|altirra` | 5200 BIOS revision | ❌ Missing |
| `-basic-rev` | `auto\|a\|b\|c\|custom\|altirra` | BASIC revision | ❌ Missing |
| `-xegame-rev` | `auto\|orig\|custom` | XEGS builtin game version | ❌ Missing |

---

## Cartridge Configuration

### Cartridge Loading
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-cart <file>` | Install cartridge (raw or CART format) | ❌ Missing |
| `-cart-type <num>` | Set cartridge type (0-160+) | ❌ Missing |
| `-cart2 <file>` | Install piggyback cartridge | ❌ Missing |
| `-cart2-type <num>` | Set piggyback cartridge type | ❌ Missing |

### Cartridge Behavior
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-cart-autoreboot` | Reboot when cartridge inserted/removed | ❌ Missing |
| `-no-cart-autoreboot` | Don't reboot after changing cartridge | ❌ Missing |

---

## Media Configuration

### Disk Images
Files specified on command line auto-mount to drives D1-D8:
- **Supported formats**: ATR, XFD, ATR.GZ, XFD.GZ, DCM, PRO, ATX
- **Maximum**: 8 disk images simultaneously
- **GUI Status**: ✅ D1-D4 Implemented, ❌ D5-D8 Missing

### Additional Disk Options (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-disk-dir <path>` | Default directory for disk images | ❌ Missing |
| `-disk-readonly` | Mount all disks as read-only | ❌ Missing |
| `-drive-split` | Enable drive splitting (D1: as 2 drives) | ❌ Missing |

### Cassette
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-tape <file>` | Insert cassette image | ✅ Implemented |
| `-boottape <file>` | Insert cassette image and boot it | ✅ Implemented |
| `-tape-readonly` | Mark cassette as read-only | ✅ Implemented |

---

## Device Configuration

### Hard Disk Emulation
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-H1 <path>` | Set path for H1: device | ✅ Implemented |
| `-H2 <path>` | Set path for H2: device | ✅ Implemented |
| `-H3 <path>` | Set path for H3: device | ✅ Implemented |
| `-H4 <path>` | Set path for H4: device | ✅ Implemented |
| `-Hpath <path>` | Set path for Atari executables on H: | ❌ Missing |
| `-Hdevicename <X>` | Use letter X for host device instead of H: | ✅ Implemented |
| `-hreadonly` | Enable read-only mode for H: device | ✅ Implemented |

### Special Devices
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-rdevice [<dev>]` | Enable R: device emulation | ✅ Implemented |
| `-netsio` | Enable NetSIO (FujiNet-PC support) | ✅ Implemented |
| `-rtime` | Enable R-Time 8 real-time clock | ✅ Implemented |
| `-nortime` | Disable R-Time 8 | ✅ Implemented |

### Printer Emulation (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-print-command <cmd>` | Command to handle printer output | ❌ Missing |
| `-print-raw` | Send raw data to printer | ❌ Missing |

---

## Hardware Extensions

### 80-Column Cards
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-xep80` | Enable XEP80 emulation | ✅ Implemented |
| `-xep80port` | Use XEP80 in port mode | ❌ Missing |
| `-af80` | Enable Austin Franklin 80-column board | ✅ Implemented |
| `-bit3` | Enable Bit3 Full View 80-column board | ✅ Implemented |

### PBI Extensions
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-1400` | Emulate Atari 1400XL | ✅ Implemented |
| `-xld` | Emulate Atari 1450XLD | ✅ Implemented |
| `-proto80` | Emulate prototype 80-column board for 1090 | ✅ Implemented |

### Voice Synthesis
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-voicebox` | Enable Voicebox speech synthesis | ✅ Implemented |

### Additional Hardware (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-black-box` | Enable Black Box hard disk interface | ❌ Missing |
| `-mio` | Enable MIO (Multi I/O) board | ❌ Missing |

---

## Performance and Timing

### Speed Control
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-turbo` | Run as fast as possible | ✅ Implemented |
| `-refresh <rate>` | Screen refresh rate | ❌ Missing |
| `-nframes <n>` | Run for n frames then exit | ❌ Missing |
| `-speed <percent>` | Set emulation speed percentage | ❌ Missing |

### Configuration
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-autosave-config` | Auto-save config on exit | ❌ Missing |
| `-no-autosave-config` | Don't auto-save config | ❌ Missing |
| `-config <file>` | Load configuration file | ❌ Missing |

### Patches
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-nopatch` | Don't patch SIO routine | ❌ Missing |
| `-nopatchall` | Don't patch OS at all (H: won't work) | ❌ Missing |
| `-enable-sio-patch` | Enable SIO patching | ✅ Implemented |

---

## Audio Configuration

### Basic Audio
| Option | Description | Values | GUI Status |
|--------|-------------|--------|------------|
| `-sound` | Enable sound | - | ✅ Implemented |
| `-nosound` | Disable sound | - | ✅ Implemented |
| `-dsprate <rate>` | Audio frequency | Hz (8000-48000) | ✅ Implemented |
| `-volume <n>` | Audio volume | 0-100 | ❌ Missing |
| `-audio16` | 16-bit audio format | - | ✅ Implemented |
| `-audio8` | 8-bit audio format | - | ✅ Implemented |
| `-snd-buflen <ms>` | Hardware buffer length | milliseconds | ❌ Missing |
| `-snddelay <ms>` | Audio latency | milliseconds | ❌ Missing |

### Stereo Audio
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-stereo` | Enable dual POKEY stereo | ✅ Implemented |
| `-nostereo` | Disable stereo | ✅ Implemented |

### Console Sounds (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-enable-sound-console` | Enable console speaker sounds | ✅ Implemented |
| `-enable-sound-sio` | Enable SIO device sounds | ✅ Implemented |

### Audio Recording (From Manual)
| Option | Description | Values | GUI Status |
|--------|-------------|--------|------------|
| `-aname <pattern>` | Audio recording filename pattern | - | ❌ Missing |
| `-ab <num>` | Audio bitrate | 8-320 kbps | ❌ Missing |
| `-ar <num>` | Audio sample rate | 8000-48000 Hz | ❌ Missing |
| `-aq <num>` | Audio quality | 0-9 | ❌ Missing |

---

## Video and Display

### Display Modes
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-fullscreen` | Run fullscreen | ✅ Implemented |
| `-windowed` | Run in window | ✅ Implemented |
| `-fs-width <num>` | Fullscreen width | ❌ Missing |
| `-fs-height <num>` | Fullscreen height | ❌ Missing |

### Screen Areas (From Manual)
| Option | Values | Description | GUI Status |
|--------|--------|-------------|------------|
| `-horiz-area` | `narrow\|tv\|full\|<number>` | Horizontal view area | ❌ Missing |
| `-vert-area` | `short\|tv\|full\|<number>` | Vertical view area | ❌ Missing |
| `-crop` | Crop display to visible area | ❌ Missing |

### Visual Effects
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-showspeed` | Show speed percentage | ✅ Implemented |
| `-showstats` | Show recording statistics | ❌ Missing |
| `-no-showstats` | Hide recording statistics | ❌ Missing |

### NTSC Color Adjustment
| Option | Description | Range | GUI Status |
|--------|-------------|-------|------------|
| `-ntsc-saturation <n>` | Color saturation | -1.0 to 1.0 | ❌ Missing |
| `-ntsc-contrast <n>` | Contrast | -1.0 to 1.0 | ❌ Missing |
| `-ntsc-brightness <n>` | Brightness | -1.0 to 1.0 | ❌ Missing |
| `-ntsc-gamma <n>` | Gamma correction | 0.1 to 4.0 | ❌ Missing |
| `-ntsc-tint <n>` | Color tint | -180 to 180 | ❌ Missing |
| `-ntsc-colordelay <n>` | GTIA color delay | 0-2 | ❌ Missing |

### PAL Color Adjustment
| Option | Description | Range | GUI Status |
|--------|-------------|-------|------------|
| `-pal-saturation <n>` | Color saturation | -1.0 to 1.0 | ❌ Missing |
| `-pal-contrast <n>` | Contrast | -1.0 to 1.0 | ❌ Missing |
| `-pal-brightness <n>` | Brightness | -1.0 to 1.0 | ❌ Missing |
| `-pal-gamma <n>` | Gamma correction | 0.1 to 4.0 | ❌ Missing |
| `-pal-tint <n>` | Color tint | -180 to 180 | ❌ Missing |
| `-pal-colordelay <n>` | GTIA color delay | 0-2 | ❌ Missing |

### External Palettes (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-palettn <file>` | Load NTSC external palette | ❌ Missing |
| `-palettn-adjust` | Apply adjustments to NTSC palette | ❌ Missing |
| `-palettep <file>` | Load PAL external palette | ❌ Missing |
| `-palettep-adjust` | Apply adjustments to PAL palette | ❌ Missing |

### Artifact Modes
| Option | Values | GUI Status |
|--------|--------|------------|
| `-ntsc-artif` | `none\|ntsc-old\|ntsc-new\|ntsc-full` | ✅ Basic Implementation |

### NTSC Filter (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-ntsc-resolution <n>` | Filter resolution | ❌ Missing |
| `-ntsc-artifacts <n>` | Luma artifacts ratio | ❌ Missing |
| `-ntsc-fringing <n>` | Chroma fringing ratio | ❌ Missing |
| `-ntsc-bleed <n>` | Color bleed amount | ❌ Missing |

### Video Recording (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-vname <pattern>` | Video recording filename pattern | ❌ Missing |
| `-video-codec <codec>` | Video codec for recording | ❌ Missing |
| `-video-bitrate <rate>` | Video bitrate for recording | ❌ Missing |

---

## Input Configuration

### Joystick Control
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-nojoystick` | Disable joystick | ❌ Missing |
| `-joy0hat` to `-joy3hat` | Use hat of joystick 0-3 | ❌ Missing |
| `-kbdjoy0` | Enable joystick 0 keyboard emulation | ❌ Missing |
| `-kbdjoy1` | Enable joystick 1 keyboard emulation | ❌ Missing |
| `-no-kbdjoy0` | Disable joystick 0 keyboard emulation | ❌ Missing |
| `-no-kbdjoy1` | Disable joystick 1 keyboard emulation | ❌ Missing |
| `-joy-distinct` | One input device per emulated stick | ❌ Missing |

### Mouse (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-grabmouse` | Prevent mouse from leaving window | ❌ Missing |
| `-mouse-device <dev>` | Specify mouse device | ❌ Missing |

### Keyboard (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-keyboardtoggle` | Enable keyboard toggle functionality | ❌ Missing |
| `-keyboard-leds` | Enable keyboard LEDs (1200XL) | ❌ Missing |

---

## Event Recording/Playback

| Option | Description | GUI Status |
|--------|-------------|------------|
| `-record <file>` | Record input to file | ❌ Missing |
| `-playback <file>` | Playback input from file | ❌ Missing |
| `-playbacknoexit` | Don't exit after playback | ❌ Missing |

---

## Debugging and Development

### Monitor
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-monitor` | Start in monitor mode | ❌ Missing |
| `-bbrk` | Break on BRK instruction | ❌ Missing |
| `-bpc <addr>` | Break on PC=address | ❌ Missing |
| `-label-file <file>` | Load monitor labels | ❌ Missing |

### File Operations
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-run <file>` | Run Atari program (COM, EXE, XEX, BAS, LST) | ❌ Missing |
| `-state <file>` | Load saved-state file | ❌ Missing |

---

## Platform-Specific Options

### X11/Linux Options (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-x11-zoom <n>` | X11 zoom factor | ❌ Missing |
| `-x11-linear` | X11 linear filtering | ❌ Missing |
| `-x11-motif` | Use Motif file selector | ❌ Missing |

### Windows Options (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-win-width <n>` | Windows display width | ❌ Missing |
| `-win-height <n>` | Windows display height | ❌ Missing |
| `-win-direct3d` | Use Direct3D renderer | ❌ Missing |

### macOS Options (From Manual)
| Option | Description | GUI Status |
|--------|-------------|------------|
| `-mac-fullscreen` | macOS native fullscreen | ❌ Missing |
| `-mac-aspect` | macOS aspect ratio correction | ❌ Missing |

---

## GUI Implementation Status

### ✅ Fully Implemented Categories
- **Machine Types**: All 10 machine types supported
- **Memory Configuration**: All major expansions (Mosaic, Axlon, MapRAM, 800 RAM)
- **Basic Audio**: Sound enable/disable, sample rates, bit depths, stereo
- **Basic Video**: Fullscreen mode, aspect ratio lock, basic artifacting
- **Media**: D1-D4 disk drives, cassette, H1-H4 hard drives
- **Hardware Extensions**: 80-column cards, PBI extensions, voice synthesis
- **ROM Configuration**: OS and BASIC ROM file selection

### ⚠️ Partially Implemented Categories
- **Video Display**: Basic options only, missing color adjustments and filters
- **Audio**: Missing volume controls, buffer settings, recording
- **Performance**: Turbo mode only, missing refresh rates and speed control

### ❌ Not Implemented Categories
- **Cartridge System**: Complete cartridge loading and management
- **Input Configuration**: Joystick and keyboard configuration
- **Color Controls**: NTSC/PAL color adjustment sliders
- **Recording/Playback**: Event recording and video/audio capture
- **Development Tools**: Monitor, debugging, state management
- **Platform-Specific**: X11, Windows, macOS specific options

---

## Priority Recommendations for GUI Enhancement

### Phase 1: Essential Features (High Priority)
1. **Cartridge System**: Loading ROM files, cartridge type selection
2. **Volume Control**: Audio volume slider and buffer settings
3. **Color Adjustments**: NTSC/PAL color control sliders
4. **D5-D8 Drives**: Complete disk drive support
5. **ROM Versions**: OS and BASIC version selection dropdowns

### Phase 2: User Experience (Medium Priority)
1. **Input Configuration**: Joystick and keyboard setup
2. **Screen Areas**: Display cropping and area selection
3. **Recording System**: Session recording and playback
4. **Configuration Profiles**: Save/load complete configurations
5. **Speed Control**: Emulation speed slider

### Phase 3: Advanced Features (Low Priority)
1. **Development Tools**: Monitor integration for debugging
2. **Video Recording**: Screen capture functionality
3. **Platform Options**: OS-specific optimizations
4. **Advanced Filters**: NTSC filter fine-tuning
5. **Specialized Hardware**: Black Box, MIO board support

This comprehensive reference now accurately reflects the full capabilities of the Atari800 emulator and provides a clear roadmap for GUI development priorities.