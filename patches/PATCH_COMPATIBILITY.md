# Fujisan Patch Compatibility

This document tracks the compatibility of Fujisan patches with specific atari800 commits.

## Current Patch Set

All patches in this directory are compatible with atari800:
- **Branch**: master (HEAD)
- **SHA**: `4804056c6a098c5deff1c628c3c081bae17199f7` (Feb 2026)
- **Note**: If patch 0003 PR is not merged upstream, pin GIT_TAG in CMakeLists.txt to a known-good SHA

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