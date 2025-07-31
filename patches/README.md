# Fujisan Patches for Atari800

This directory contains patches that modify the vanilla atari800 source code to support Fujisan features.

## Patches

### Core Functionality
1. **libatari800-api-disk-functions.patch** - Adds disk management API functions to libatari800
2. **libatari800-header-disk-api.patch** - Adds disk API declarations to libatari800 header

### NetSIO/FujiNet Support
3. **netsio-sio-integration.patch** - Main SIO system integration with NetSIO for FujiNet support
4. **netsio-sio-header.patch** - SIO header updates for NetSIO integration  
5. **netsio-local-disk-priority.patch** - NetSIO enhancements for local disk priority

### Disk Activity Monitoring
6. **sio-disk-activity.patch** - Disk activity tracking for LED indicators
   - Note: This patch adds the callback invocations to sio.c that trigger disk activity LEDs
   - Without this patch, the disk activity API will exist but LEDs won't light up
7. **sio-header-update.patch** - Additional SIO header updates

### UI Updates
8. **ui-netsio-indicator.patch** - UI updates to show NetSIO status

## Installation

### Method 1: Using the apply-patches script

```bash
export ATARI800_SRC_PATH=/path/to/atari800-src
cd fujisan/patches
./apply-patches.sh
```

### Method 2: Manual application

```bash
cd /path/to/atari800-src
patch -p0 < /path/to/fujisan/patches/libatari800-api-disk-functions.patch
patch -p0 < /path/to/fujisan/patches/libatari800-header-disk-api.patch
patch -p0 < /path/to/fujisan/patches/netsio-sio-integration.patch
patch -p0 < /path/to/fujisan/patches/netsio-sio-header.patch
patch -p0 < /path/to/fujisan/patches/netsio-local-disk-priority.patch
patch -p0 < /path/to/fujisan/patches/sio-disk-activity.patch
patch -p0 < /path/to/fujisan/patches/sio-header-update.patch
patch -p0 < /path/to/fujisan/patches/ui-netsio-indicator.patch
```

## Building After Patching

1. **Build libatari800**:
   ```bash
   cd $ATARI800_SRC_PATH
   ./configure --target=libatari800
   make
   ```

2. **Build Fujisan**:
   ```bash
   export ATARI800_SRC_PATH=/path/to/atari800-src
   cd /path/to/fujisan
   cmake .
   make
   ```

## Reverting Patches

To revert patches and restore original files:

```bash
cd $ATARI800_SRC_PATH
git checkout -- src/netsio.c src/sio.c src/sio.h src/ui.c
```

## Technical Details

See `README-NetSIO-FujiNet-Support.md` for detailed information about the NetSIO/FujiNet patches.