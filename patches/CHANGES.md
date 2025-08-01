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