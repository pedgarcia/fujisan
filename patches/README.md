# Fujisan Patches for Atari800

This directory contains patches that modify the vanilla atari800 source code to support Fujisan features.

## Update (January 2025)

The patches have been updated to use Git format-patch for better compatibility and easier application.
The old patch files have been moved to the `old_patches/` directory for reference.

## Current Patches

### Core Functionality
1. **0001-libatari800-disk-api.patch** - Adds disk management and activity API functions to libatari800
   - Adds `libatari800_dismount_disk_image()` and `libatari800_disable_drive()` functions
   - Adds disk activity monitoring API for LED indicators
   - Adds SIO patch control API for disk speed management
   - Modifies both `src/libatari800/api.c` and `src/libatari800/libatari800.h`

## Prerequisites

Before building atari800 with these patches, ensure you have the following installed:

- **autoconf** - Required for generating configure scripts
- **automake** - Required for generating Makefiles
- **git** - Required for applying git format patches

On macOS with Homebrew:
```bash
brew install autoconf automake
```

On Ubuntu/Debian:
```bash
sudo apt install autoconf automake
```

## Installation

### Method 1: Using the apply-patches script

```bash
export ATARI800_SRC_PATH=/path/to/atari800-src
cd fujisan/patches
./apply-patches.sh
```

### Method 2: Manual application with Git

```bash
cd /path/to/atari800-src
git am /path/to/fujisan/patches/0001-libatari800-disk-api.patch
```

### Method 3: Manual application with patch

```bash
cd /path/to/atari800-src
patch -p1 < /path/to/fujisan/patches/0001-libatari800-disk-api.patch
```

## Building After Patching

1. **Generate build files** (if not already present):
   ```bash
   cd $ATARI800_SRC_PATH
   ./autogen.sh
   ```

2. **Build libatari800**:
   ```bash
   cd $ATARI800_SRC_PATH
   ./configure --target=libatari800
   make
   ```

3. **Build Fujisan**:
   ```bash
   export ATARI800_SRC_PATH=/path/to/atari800-src
   cd /path/to/fujisan
   
   # For macOS with Homebrew Qt5:
   ATARI800_SRC_PATH=/path/to/atari800-src CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5" cmake .
   
   # For other systems:
   cmake .
   
   make
   ```

## Reverting Patches

To revert patches and restore original files:

### If using Git:
```bash
cd $ATARI800_SRC_PATH
git checkout -- src/libatari800/api.c src/libatari800/libatari800.h
```

### If not using Git:
```bash
cd $ATARI800_SRC_PATH
# Restore from backup files if you made them
cp src/libatari800/api.c.orig src/libatari800/api.c
cp src/libatari800/libatari800.h.orig src/libatari800/libatari800.h
```

## Troubleshooting

### "configure: error: cannot find required auxiliary files"
This happens when autotools files are missing. Run `./autogen.sh` first.

### "Qt5 not found" during Fujisan build
Make sure to set CMAKE_PREFIX_PATH to your Qt5 installation:
- macOS: `CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@5"`
- Linux: Install qt5-dev packages
- Windows: Set path to Qt5 installation

### Build warnings about ULONG redefinition
These are harmless warnings about macro redefinitions between atari.h and libatari800.h.

## Notes

- The disk activity monitoring requires the callback to be invoked from within the SIO emulation
- Currently, atari800 already includes the necessary SIO_last_op* variables for disk activity tracking
- The patch only adds the libatari800 API layer to expose this functionality