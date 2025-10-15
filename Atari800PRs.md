# Atari800 Upstream PR Tracking

This document tracks patches from Fujisan that are being submitted to upstream atari800/atari800.

## Submitted PRs

### PR #252: Disk Management and Activity Monitoring API ✅
- **Status**: ✅ Merged (October 13, 2025)
- **URL**: https://github.com/atari800/atari800/pull/252
- **Branch**: `feature/libatari800-disk-api`
- **Date Submitted**: October 12, 2025
- **Date Merged**: October 13, 2025
- **Merge Commit**: `fcb6e799734c749f9e326640f4d506abf854e95c`
- **Patch File**: `patches/0001-libatari800-disk-api.patch` (no longer applied - now in upstream)

**What it adds**:
- `libatari800_mount_disk()` - Mount disk images
- `libatari800_unmount_disk()` - Unmount disk images
- `libatari800_disable_drive()` - Disable drives
- `libatari800_set_disk_activity_callback()` - Real-time disk I/O callbacks
- `libatari800_get_sio_patch_enabled()` - Query SIO patch status
- `libatari800_set_sio_patch_enabled()` - Control disk speed

**Files changed**:
- `src/libatari800/api.c` (+52 lines)
- `src/libatari800/libatari800.h` (+10 lines)
- `src/sio.c` (+31 lines)

**Build status**: ✅ Builds cleanly

---

## Pending Patches (Not Yet Submitted)

### Patch 0002: Windows ULONG Type Conflicts
- **Patch File**: `patches/0002-windows-ulong-conflicts.patch`
- **Status**: Ready for submission (needs Windows build testing first)
- **Priority**: Medium
- **Issue**: Windows.h defines ULONG as unsigned long (8 bytes on 64-bit), atari800 expects 4 bytes
- **Fix**: Force correct 4-byte definition after Windows.h inclusion
- **Also removes**: Duplicate ANTIC lookup table declarations

**Next steps**:
1. Test on real Windows build (MinGW-w64, MSVC)
2. Document specific compilation errors if found
3. Consider splitting ULONG fix from antic.c changes

---

### Patch 0003: Single-Instruction Stepping
- **Patch File**: `patches/0003-single-instruction-stepping.patch`
- **Status**: Needs RFC/Discussion first
- **Priority**: Low-Medium
- **What it adds**:
  - `libatari800_step_instruction()` - Execute exactly one 6502 instruction
  - Exposes `MONITOR_break_step` for libatari800
  - `CPU_GetInstructionCycles()` for cycle tracking

**Rationale**:
- Current limitation: can only step full frames (~29,000 cycles)
- Makes libatari800-based debuggers practical
- Uses existing monitor infrastructure

**Next steps**:
1. Open GitHub Discussion/Issue first
2. Title: "RFC: Add instruction-level stepping to libatari800 API"
3. Get maintainer feedback on approach
4. Submit PR if approved

---

### Patch 0004: Partial Frame Execution
- **Patch File**: `patches/0004-partial-frame-execution.patch`
- **Status**: Needs refinement before submission
- **Priority**: Low
- **What it adds**:
  - `libatari800_execute_cycles(int target_cycles)` - Execute specific cycle count
  - Improves breakpoint precision from ~29,000 to ~500 cycles

**Issues to fix**:
1. **Hardcoded PAL values** (line 65: `if (ANTIC_ypos >= 312)`)
   - NTSC is 262 scanlines, not 312
   - Should use `Atari800_tv_mode` for detection
2. **Direct ANTIC manipulation** - bypasses normal processing
3. **API design** - should it return actual cycles executed?

**Next steps**:
1. Fix NTSC/PAL detection
2. Test with different machine types
3. Consider submitting AFTER patch 0003 accepted (related feature)

---

## PR Strategy

### Phase 1: Core Functionality (✅ Complete and Merged)
- ✅ Patch 0001 submitted as PR #252 - **MERGED Oct 13, 2025**

### Phase 2: Bug Fixes (If applicable)
- ⏳ Patch 0002 - Windows ULONG (if verified as real issue)

### Phase 3: Advanced Features (After discussion)
- ⏳ Patch 0003 - Single stepping (needs RFC)
- ⏳ Patch 0004 - Partial frame execution (needs refinement + RFC)

---

## Notes

- All patches are Fujisan-specific initially but provide general value to libatari800 users
- Changes use `#ifdef LIBATARI800` guards to avoid affecting standard builds
- Patches are tested and working in Fujisan Qt frontend
- Commit messages follow upstream style (no Claude/AI references per CLAUDE.md)

---

## Upstream Repository Info

- **Upstream**: https://github.com/atari800/atari800
- **Fork**: https://github.com/pedgarcia/atari800
- **Local**: `/Users/pgarcia/dev/atari/atari800`
- **Compatible with**: atari800 commit `fcb6e79` (Oct 13, 2025) - includes PR #252

---

Last updated: October 14, 2025
