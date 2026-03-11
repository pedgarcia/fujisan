# Patch System Changes

## 0010-netsio-windows-support.patch (NetSIO on Windows)

Adds FujiNet (NetSIO) support for the atari800 Windows build by providing a Win32-native implementation in `netsiowin.c` (Winsock2, CreateThread, ring-buffer FIFO). The build uses **either** `netsio.c` (POSIX) **or** `netsiowin.c` (Windows), never both.

**Contents:** Patches `configure.ac` (enable NetSIO on Windows, link ws2_32 instead of pthread), `src/Makefile.am` (compile netsiowin.c when building for Windows), `src/netsio.h` (Windows-safe includes and `ssize_t`), and adds `src/netsiowin.c`.

**Phase 1 (vanilla atari800):** Apply to a standalone atari800 tree on Windows (e.g. from atari800 root: `git apply path/to/0010-netsio-windows-support.patch` or set `ATARI800_SRC_PATH` and run `apply-patches.sh`). Then `./configure --enable-netsio`, `make`, and run `atari800.exe -netsio 9997` to test. The script skips this patch on non-Windows unless cross-compiling (patch name contains "windows").

**Phase 2:** After Phase 1 is validated, enable the NetSIO/FujiNet UI in Fujisan on Windows (remove or relax `#ifndef Q_OS_WIN` guards).

---

## 0009-remove-netsio-cold-reset-from-coldstart.patch (March 2025)

Removes `netsio_cold_reset(0xFF)` entirely from `Atari800_Coldstart()` in the
atari800 core. `Atari800_Coldstart()` is called from many internal paths:
`BINLOAD_Loader()` (XEX loading), `libatari800_reboot_with_file()` (debug XEX
load), cartridge changes, `coldRestart()` (system.restart TCP command), and
more. In every one of these cases, the unconditional `netsio_cold_reset()`
fired a full FujiNet-PC process restart as a surprise side-effect with no
waiting for FujiNet to come back online.

Fujisan's `AtariEmulator::coldBoot()` already handles FujiNet coordination
explicitly — it sends `netsio_cold_reset(0xFF)`, waits up to 3 seconds for
FujiNet to reconnect, and only then calls `Atari800_Coldstart()`. The call
inside `Atari800_Coldstart()` was therefore always a redundant second reset.

After this patch, `netsio_cold_reset()` is only sent when Fujisan's `coldBoot()`
explicitly triggers it (i.e. the user calls `system.cold_boot` via TCP or
through the UI). All other cold-start paths leave FujiNet running undisturbed.

---

# Patch System Changes - January 2025

## Summary of Changes

This document describes the changes made to the Fujisan patch system to improve reliability and compatibility.

## Changes Made

### 1. Converted to Git Format Patches
- Replaced traditional unified diff patches with Git format-patch files
- The new format is more reliable and works with both `git am` and `patch -p1`
- Old patches moved to `old_patches/` directory for reference

### 2. Consolidated Patches
- Combined the separate `libatari800-api-disk-functions.patch` and `libatari800-header-disk-api.patch` into a single patch: `0001-libatari800-disk-api.patch`
- Removed unnecessary patches (sio.h changes were already in upstream)

### 3. Updated Documentation
- Added autoconf/automake to prerequisites in both README files
- Added troubleshooting section for common build issues
- Clarified environment variable usage
- Added platform-specific instructions for installing autotools

### 4. Updated Apply Script
- Modified `apply-patches.sh` to detect Git repositories and use appropriate commands
- Added support for both `git am` (for Git repos) and `patch -p1` (for non-Git)
- Added reminder to run `autogen.sh` if needed

## Files Modified

1. `/patches/README.md` - Completely updated with new patch information
2. `/patches/apply-patches.sh` - Updated to support Git format patches
3. `/README.md` - Added autoconf/automake to prerequisites
4. `/patches/0001-libatari800-disk-api.patch` - New Git format patch (replaces old patches)

## Files Moved

All old patch files moved to `/patches/old_patches/`:
- `libatari800-api-disk-functions.patch`
- `libatari800-header-disk-api.patch`
- `netsio-local-disk-priority.patch`
- `netsio-sio-header.patch`
- `netsio-sio-integration.patch`
- `sio-disk-activity.patch`
- `sio-header-update.patch`
- `ui-netsio-indicator.patch`

## Rationale

1. **Git Format Benefits**:
   - Better metadata preservation (author, date, commit message)
   - More reliable application with `git am`
   - Still works with traditional `patch` command
   - Easier to manage and track changes

2. **Consolidation**:
   - Single patch for related changes is easier to manage
   - Reduces chance of partial application failures

3. **Documentation Updates**:
   - Missing autotools dependency was a common build failure
   - Clearer instructions reduce support burden

## Testing

The new patch system was tested by:
1. Applying to a fresh atari800 checkout
2. Building libatari800 successfully
3. Building and running Fujisan successfully

## Recommendations for Future

1. Consider creating a build script that automates the entire process
2. Look into GitHub Actions for automated testing of patches
3. Consider upstreaming the libatari800 disk API changes to atari800 project