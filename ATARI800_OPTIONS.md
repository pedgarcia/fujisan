# Fujisan - Complete Atari800 Command Line Options Reference

This document provides a comprehensive mapping of ALL command line options and configuration settings available in the Atari800 emulator. This reference is useful for understanding what features could be integrated into GUI frontends and for advanced configuration.

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
- [Configuration File Parameters](#configuration-file-parameters)
- [Usage Examples](#usage-examples)

---

## Machine Configuration

### Machine Types
| Option | Description | RAM | BASIC | Notes |
|--------|-------------|-----|-------|-------|
| `-atari` | Atari 400/800 | 48K | No | Original model |
| `-1200` | Atari 1200XL | 64K | No | Keyboard LEDs, F-keys |
| `-xl` | Atari 800XL | 64K | Yes | Most common model |
| `-xe` | Atari 130XE | 128K | Yes | Extended memory |
| `-320xe` | Atari 320XE | 320K | Yes | Compy-Shop variant |
| `-rambo` | Atari 320XE | 320K | Yes | Rambo XL variant |
| `-576xe` | Atari 576XE | 576K | Yes | Large memory |
| `-1088xe` | Atari 1088XE | 1088K | Yes | Maximum memory |
| `-xegs` | Atari XEGS | 64K | Yes | Game system with built-in game |
| `-5200` | Atari 5200 | 16K | No | Game console |

### TV Mode
| Option | Description | Frequency | Scanlines |
|--------|-------------|-----------|-----------|
| `-pal` | PAL TV mode | 50Hz | 312 |
| `-ntsc` | NTSC TV mode | 60Hz | 262 |

### Memory Configuration
| Option | Description |
|--------|-------------|
| `-c` | Enable RAM between 0xc000-0xcfff in Atari 800 |
| `-mosaic <n>` | Mosaic memory expansion: n KB total RAM |
| `-axlon <n>` | Axlon memory expansion: n KB total RAM |
| `-axlon0f` | Use Axlon shadow at 0x0fc0-0x0fff |
| `-mapram` | Enable MapRAM for XL/XE |
| `-no-mapram` | Disable MapRAM |

### BASIC Configuration
| Option | Description |
|--------|-------------|
| `-basic` | Enable Atari BASIC ROM |
| `-nobasic` | Disable Atari BASIC ROM |

---

## ROM Configuration

### ROM Files
| Option | Description | Machine | Size |
|--------|-------------|---------|------|
| `-osa_rom <file>` | OS A ROM | 400/800 | 10KB |
| `-osb_rom <file>` | OS B ROM | 400/800 | 10KB |
| `-xlxe_rom <file>` | XL/XE OS ROM | XL/XE | 16KB |
| `-5200_rom <file>` | 5200 BIOS ROM | 5200 | 2KB |
| `-basic_rom <file>` | BASIC ROM | All 8-bit | 8KB |

### ROM Versions
| Option | Values | Description |
|--------|--------|-------------|
| `-800-rev` | `auto\|a-ntsc\|a-pal\|b-ntsc\|custom\|altirra` | 400/800 OS revision |
| `-xl-rev` | `auto\|10\|11\|1\|2\|3a\|3b\|5\|3\|4\|59\|59a\|custom\|altirra` | XL/XE OS revision |
| `-5200-rev` | `auto\|orig\|a\|custom\|altirra` | 5200 BIOS revision |
| `-basic-rev` | `auto\|a\|b\|c\|custom\|altirra` | BASIC revision |
| `-xegame-rev` | `auto\|orig\|custom` | XEGS builtin game version |

---

## Cartridge Configuration

### Cartridge Loading
| Option | Description |
|--------|-------------|
| `-cart <file>` | Install cartridge (raw or CART format) |
| `-cart-type <num>` | Set cartridge type (0-70+) |
| `-cart2 <file>` | Install piggyback cartridge |
| `-cart2-type <num>` | Set piggyback cartridge type |

### Cartridge Behavior
| Option | Description |
|--------|-------------|
| `-cart-autoreboot` | Reboot when cartridge inserted/removed |
| `-no-cart-autoreboot` | Don't reboot after changing cartridge |

---

## Media Configuration

### Disk Images
Files specified on command line auto-mount to drives D1-D8:
- **Supported formats**: ATR, XFD, ATR.GZ, XFD.GZ, DCM, PRO, ATX
- **Maximum**: 8 disk images simultaneously
- **Example**: `atari800 disk1.atr disk2.atr disk3.atr`

### Cassette
| Option | Description |
|--------|-------------|
| `-tape <file>` | Insert cassette image |
| `-boottape <file>` | Insert cassette image and boot it |
| `-tape-readonly` | Mark cassette as read-only |

---

## Device Configuration

### Hard Disk Emulation
| Option | Description |
|--------|-------------|
| `-H1 <path>` | Set path for H1: device |
| `-H2 <path>` | Set path for H2: device |
| `-H3 <path>` | Set path for H3: device |
| `-H4 <path>` | Set path for H4: device |
| `-Hpath <path>` | Set path for Atari executables on H: |
| `-Hdevicename <X>` | Use letter X for host device instead of H: |
| `-hreadonly` | Enable read-only mode for H: device |

### Special Devices
| Option | Description |
|--------|-------------|
| `-rdevice [<dev>]` | Enable R: device emulation |
| `-netsio` | Enable NetSIO (FujiNet-PC support) |
| `-rtime` | Enable R-Time 8 real-time clock |
| `-nortime` | Disable R-Time 8 |

---

## Hardware Extensions

### 80-Column Cards
| Option | Description |
|--------|-------------|
| `-xep80` | Enable XEP80 emulation |
| `-xep80port` | Use XEP80 in port mode |
| `-af80` | Enable Austin Franklin 80-column board |
| `-bit3` | Enable Bit3 Full View 80-column board |

### PBI Extensions
| Option | Description |
|--------|-------------|
| `-1400` | Emulate Atari 1400XL |
| `-xld` | Emulate Atari 1450XLD |
| `-proto80` | Emulate prototype 80-column board for 1090 |

### Voice Synthesis
| Option | Description |
|--------|-------------|
| `-voicebox` | Enable Voicebox speech synthesis |

---

## Performance and Timing

### Speed Control
| Option | Description |
|--------|-------------|
| `-turbo` | Run as fast as possible |
| `-refresh <rate>` | Screen refresh rate |
| `-nframes <n>` | Run for n frames then exit |

### Configuration
| Option | Description |
|--------|-------------|
| `-autosave-config` | Auto-save config on exit |
| `-no-autosave-config` | Don't auto-save config |

### Patches
| Option | Description |
|--------|-------------|
| `-nopatch` | Don't patch SIO routine |
| `-nopatchall` | Don't patch OS at all (H: won't work) |

---

## Audio Configuration

### Basic Audio
| Option | Description | Values |
|--------|-------------|--------|
| `-sound` | Enable sound | - |
| `-nosound` | Disable sound | - |
| `-dsprate <rate>` | Audio frequency | Hz (8000-48000) |
| `-volume <n>` | Audio volume | 0-100 |
| `-audio16` | 16-bit audio format | - |
| `-audio8` | 8-bit audio format | - |
| `-snd-buflen <ms>` | Hardware buffer length | milliseconds |
| `-snddelay <ms>` | Audio latency | milliseconds |

### Stereo Audio
| Option | Description |
|--------|-------------|
| `-stereo` | Enable dual POKEY stereo |
| `-nostereo` | Disable stereo |

### Audio Recording
| Option | Description | Values |
|--------|-------------|--------|
| `-aname <pattern>` | Audio recording filename pattern | - |
| `-ab <num>` | Audio bitrate | 8-320 kbps |
| `-ar <num>` | Audio sample rate | 8000-48000 Hz |
| `-aq <num>` | Audio quality | 0-9 |

---

## Video and Display

### Display Modes
| Option | Description |
|--------|-------------|
| `-fullscreen` | Run fullscreen |
| `-windowed` | Run in window |
| `-fs-width <num>` | Fullscreen width |
| `-fs-height <num>` | Fullscreen height |

### Screen Areas
| Option | Values | Description |
|--------|--------|-------------|
| `-horiz-area` | `narrow\|tv\|full\|<number>` | Horizontal view area |
| `-vert-area` | `short\|tv\|full\|<number>` | Vertical view area |

### Visual Effects
| Option | Description |
|--------|-------------|
| `-showspeed` | Show speed percentage |
| `-showstats` | Show recording statistics |
| `-no-showstats` | Hide recording statistics |

### NTSC Color Adjustment
| Option | Description | Range |
|--------|-------------|-------|
| `-ntsc-saturation <n>` | Color saturation | -1.0 to 1.0 |
| `-ntsc-contrast <n>` | Contrast | -1.0 to 1.0 |
| `-ntsc-brightness <n>` | Brightness | -1.0 to 1.0 |
| `-ntsc-gamma <n>` | Gamma correction | 0.1 to 4.0 |
| `-ntsc-tint <n>` | Color tint | -180 to 180 |
| `-ntsc-colordelay <n>` | GTIA color delay | 0-2 |

### PAL Color Adjustment
| Option | Description | Range |
|--------|-------------|-------|
| `-pal-saturation <n>` | Color saturation | -1.0 to 1.0 |
| `-pal-contrast <n>` | Contrast | -1.0 to 1.0 |
| `-pal-brightness <n>` | Brightness | -1.0 to 1.0 |
| `-pal-gamma <n>` | Gamma correction | 0.1 to 4.0 |
| `-pal-tint <n>` | Color tint | -180 to 180 |
| `-pal-colordelay <n>` | GTIA color delay | 0-2 |

### External Palettes
| Option | Description |
|--------|-------------|
| `-palettn <file>` | Load NTSC external palette |
| `-palettn-adjust` | Apply adjustments to NTSC palette |
| `-palettep <file>` | Load PAL external palette |
| `-palettep-adjust` | Apply adjustments to PAL palette |

### Artifact Modes
| Option | Values |
|--------|--------|
| `-ntsc-artif` | `none\|ntsc-old\|ntsc-new\|ntsc-full` |

### NTSC Filter
| Option | Description |
|--------|-------------|
| `-ntsc-resolution <n>` | Filter resolution |
| `-ntsc-artifacts <n>` | Luma artifacts ratio |
| `-ntsc-fringing <n>` | Chroma fringing ratio |
| `-ntsc-bleed <n>` | Color bleed amount |

### Video Recording
| Option | Description |
|--------|-------------|
| `-vname <pattern>` | Video recording filename pattern |

---

## Input Configuration

### Joystick Control
| Option | Description |
|--------|-------------|
| `-nojoystick` | Disable joystick |
| `-joy0hat` to `-joy3hat` | Use hat of joystick 0-3 |
| `-kbdjoy0` | Enable joystick 0 keyboard emulation |
| `-kbdjoy1` | Enable joystick 1 keyboard emulation |
| `-no-kbdjoy0` | Disable joystick 0 keyboard emulation |
| `-no-kbdjoy1` | Disable joystick 1 keyboard emulation |
| `-joy-distinct` | One input device per emulated stick |

### Mouse
| Option | Description |
|--------|-------------|
| `-grabmouse` | Prevent mouse from leaving window |

### Linux-Specific Input
| Option | Description |
|--------|-------------|
| `-joy0 <pathname>` | Select LPTjoy0 device |
| `-joy1 <pathname>` | Select LPTjoy1 device |

---

## Event Recording/Playback

| Option | Description |
|--------|-------------|
| `-record <file>` | Record input to file |
| `-playback <file>` | Playback input from file |
| `-playbacknoexit` | Don't exit after playback |

---

## Debugging and Development

### Monitor
| Option | Description |
|--------|-------------|
| `-monitor` | Start in monitor mode |
| `-bbrk` | Break on BRK instruction |
| `-bpc <addr>` | Break on PC=address |
| `-label-file <file>` | Load monitor labels |

### File Operations
| Option | Description |
|--------|-------------|
| `-run <file>` | Run Atari program (COM, EXE, XEX, BAS, LST) |
| `-state <file>` | Load saved-state file |

---

## Configuration File Parameters

### Directory Settings
```
ATARI_FILES_DIR=<path>          # Directory for Atari files
SAVED_FILES_DIR=<path>          # Directory for saved files
H1_DIR through H4_DIR           # Hard disk device directories
SHOW_HIDDEN_FILES=0|1           # Show hidden files in selector
```

### Machine Settings
```
MACHINE_TYPE=Atari 400/800|Atari XL/XE|Atari 5200
RAM_SIZE=<number>|320 (RAMBO)|320 (COMPY SHOP)
DEFAULT_TV_MODE=PAL|NTSC
BUILTIN_BASIC=0|1
KEYBOARD_LEDS=0|1
F_KEYS=0|1
BUILTIN_GAME=0|1
KEYBOARD_DETACHED=0|1
1200XL_JUMPER=0|1
```

### Performance Settings
```
SCREEN_REFRESH_RATIO=<number>
ACCURATE_SKIPPED_FRAMES=0|1
DISABLE_BASIC=0|1
TURBO_SPEED=<percentage>
```

### Patches and Devices
```
ENABLE_SIO_PATCH=0|1
ENABLE_SLOW_XEX_LOADING=0|1
ENABLE_H_PATCH=0|1
ENABLE_P_PATCH=0|1
ENABLE_R_PATCH=0|1
HD_READ_ONLY=<bitmask>
HD_DEVICE_NAME=<letter>
PRINT_COMMAND=<command>
```

### Memory Extensions
```
MOSAIC_RAM_NUM_BANKS=<number>
AXLON_RAM_NUM_BANKS=<number>
ENABLE_MAPRAM=0|1
```

### Audio Settings
```
ENABLE_NEW_POKEY=0|1
STEREO_POKEY=0|1
SPEAKER_SOUND=0|1
```

### System Settings
```
CFG_SAVE_ON_EXIT=0|1
```

---

## Usage Examples

### Basic Usage
```bash
# Simple 800XL with disk
atari800 -xl game.atr

# Atari 800 with cartridge
atari800 -atari -cart basic.rom

# Multiple disks auto-mounted to D1-D8
atari800 disk1.atr disk2.atr disk3.atr
```

### Advanced Configuration
```bash
# NTSC 130XE with stereo and fullscreen
atari800 -xe -ntsc -stereo -fullscreen

# Maximum performance setup
atari800 -turbo -nosound -refresh 1

# Recording and playback
atari800 -record session.inp program.xex
atari800 -playback session.inp
```

### Development Setup
```bash
# Start with monitor and break on BRK
atari800 -monitor -bbrk program.xex

# Load state and run specific program
atari800 -state save.a8s -run test.com
```

### Complex Media Setup
```bash
# Everything enabled
atari800 -xe -pal -cart game.car -H1 /home/user/atari \
         -stereo -fullscreen -ntsc-artif ntsc-full \
         -dsprate 48000 -volume 80 disk1.atr disk2.atr
```

### Hardware Extensions
```bash
# 80-column setup with voice
atari800 -xl -xep80 -voicebox

# PBI extensions
atari800 -1400 -rtime -af80
```

---

## Implementation Notes for GUI Frontends

### High Priority for GUI Integration
- Machine types and TV modes
- ROM file selection and versions
- BASIC enable/disable
- Audio settings (volume, stereo)
- Display settings (fullscreen, screen areas)
- Cartridge loading
- Disk image mounting

### Medium Priority
- Hard disk device configuration
- Color adjustment controls
- Performance settings
- Input device configuration

### Advanced/Debug Features
- Monitor and debugging options
- Event recording/playback
- Hardware extensions
- NTSC filter parameters

### Configuration File Integration
Consider implementing a configuration manager that can:
- Load/save `.atari800.cfg` files
- Convert between command-line arguments and config parameters
- Provide preset configurations for common setups

This comprehensive reference covers all 150+ command line options and configuration parameters available in the Atari800 emulator, providing a complete foundation for advanced GUI development and power-user configuration.