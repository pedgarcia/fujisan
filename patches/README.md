# Fujisan Patches for Atari800

This directory contains patches that modify the vanilla atari800 source code to support Fujisan features.

## Patches

1. **netsio-sio-integration.patch** - Main SIO system integration with NetSIO for FujiNet support
2. **netsio-sio-header.patch** - SIO header updates for NetSIO integration  
3. **netsio-local-disk-priority.patch** - NetSIO enhancements for local disk priority
4. **sio-disk-activity.patch** - Disk activity tracking for LED indicators
5. **sio-header-update.patch** - Additional SIO header updates
6. **ui-netsio-indicator.patch** - UI updates to show NetSIO status

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