# Fujisan Patch Compatibility

This document tracks the compatibility of Fujisan patches with specific atari800 commits.

## Current Patch Set

All patches in this directory are compatible with atari800 commit:
- **SHA**: `575a943b3523bf0f5c9b73ba8641d83cac672f24`
- **Message**: kbhits is const for SDL2 only
- **Date**: As of July 31, 2025

## Required Patches

### libatari800-disk-api.patch
Adds essential libatari800 API functions for:
- Disk management (dismount/disable drives)
- Disk activity monitoring with callbacks
- SIO patch control (fast/realistic disk timing)

### NetSIO/FujiNet Support Patches
See README-NetSIO-FujiNet-Support.md for details on network SIO patches.

## Applying Patches

Use the provided `apply-patches.sh` script:

```bash
export ATARI800_SRC_PATH=/path/to/atari800-src
./apply-patches.sh
```

## Testing Compatibility

When updating to a new atari800 commit:
1. Note the current atari800 commit SHA
2. Apply patches and test build
3. If patches fail, update them and document the new compatible SHA
4. Update this file with the new compatibility information