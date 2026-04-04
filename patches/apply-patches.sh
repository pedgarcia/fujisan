#!/bin/bash
# Apply Fujisan patches to atari800 source

if [ -z "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH environment variable not set"
    echo "Please set it to your atari800 source directory"
    exit 1
fi

if [ ! -d "$ATARI800_SRC_PATH" ]; then
    echo "Error: ATARI800_SRC_PATH directory does not exist: $ATARI800_SRC_PATH"
    exit 1
fi

PATCHES_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ATARI800_SRC_PATH" || exit 1

echo "Applying patches to atari800 source at: $ATARI800_SRC_PATH"

# Avoid re-applying patches on configure reruns in the same source tree.
# ExternalProject may invoke configure multiple times without recloning.
PATCH_MARKER=".fujisan-patches-applied"
if [ -f "$PATCH_MARKER" ]; then
    echo "Patches already applied in this source tree ($PATCH_MARKER present), skipping."
    exit 0
fi

# Bootstrap detection for trees patched before marker support existed.
# Require 0003 (single-step) markers too — otherwise configure reruns can skip
# an incomplete tree that only had later NetSIO / execute_cycles hunks applied.
if [ -f "src/netsio.c" ] && [ -f "src/libatari800/api.c" ] && \
   grep -q 'NETSIO_RECV_BYTE_TIMEOUT_SEC' src/netsio.c && \
   grep -q 'netsio_sync_mutex' src/netsio.c && \
   grep -q 'int libatari800_execute_cycles(int target_cycles)' src/libatari800/api.c && \
   grep -q 'CPU_GetInstructionCycles' src/cpu.h; then
    echo "Detected previously patched source tree; writing $PATCH_MARKER and skipping."
    touch "$PATCH_MARKER"
    exit 0
fi

# Check if this is a git repository
if [ -d .git ]; then
    echo "Git repository detected, using 'git am' for patches"

    # On native Windows (MSYS2/MinGW), git defaults to core.autocrlf=true which
    # checks out source files with CRLF endings. Patches were created with LF
    # context lines, so git apply fails even though the content matches. Fix this
    # once for the whole tree before applying any patches.
    if [[ "$MSYSTEM" != "" ]] || [[ "$OSTYPE" == "msys" ]]; then
        echo "Windows detected: forcing LF line endings in source tree before patching"
        git config core.autocrlf false
        git config core.eol lf
        git checkout -- .
    fi

    for patch in "$PATCHES_DIR"/0*.patch; do
        if [ -f "$patch" ]; then
            patch_name="$(basename "$patch")"
            echo "Applying patch: $patch_name"

            # Skip patch 0001 - merged into upstream as PR #252 (Oct 13, 2025)
            if [[ "$patch_name" == "0001-libatari800-disk-api.patch" ]]; then
                echo "Skipping patch 0001 - already in upstream (PR #252 merged)"
                continue
            fi

            # Skip patches 0005 and 0006 - accepted into atari800 upstream
            if [[ "$patch_name" == "0005-netsio-so-reuseaddr.patch" ]] || [[ "$patch_name" == "0006-netsio-shutdown.patch" ]]; then
                echo "Skipping patch $patch_name - already in upstream"
                continue
            fi

            # Skip Windows-specific patches on non-Windows systems
            # Allow Windows patches when cross-compiling (CC contains mingw)
            if [[ "$patch_name" == *"windows"* ]] && [[ "$OSTYPE" != "msys" ]] && [[ "$MSYSTEM" == "" ]] && [[ "$CC" != *"mingw"* ]]; then
                echo "Skipping Windows-specific patch on non-Windows system"
                continue
            fi
            
            # Try git apply first (more reliable than git am for patches)
            # Use --3way for better conflict resolution and avoid prompts.
            # If reverse-check succeeds, the patch is already present: skip.
            if git apply --check "$patch" 2>/dev/null; then
                # Apply to the working tree first.  git apply --3way can fail with
                # "does not match index" after prior patches updated files without
                # updating the index (common for ExternalProject checkouts).
                if ! git apply "$patch" </dev/null 2>/tmp/fujisan-git-apply.err; then
                    if ! git apply --3way "$patch" </dev/null 2>/dev/null; then
                        echo "Error: git apply failed for $patch_name"
                        cat /tmp/fujisan-git-apply.err 2>/dev/null || true
                        exit 1
                    fi
                fi
                echo "✓ Patch applied successfully with git apply"
            elif git apply --reverse --check "$patch" 2>/dev/null; then
                echo "✓ Patch $patch_name already applied (skipping)"
            elif patch -p1 --dry-run --force < "$patch" >/dev/null 2>&1; then
                if ! patch -p1 --force --no-backup-if-mismatch < "$patch" </dev/null; then
                    echo "Error: patch command failed for $patch_name"
                    exit 1
                fi
                echo "✓ Patch applied successfully with patch command"
            else
                # For critical patch 0007 (BINLOAD/NetSIO), apply manually
                if [[ "$patch_name" == "0007-netsio-binload-priority.patch" ]]; then
                    echo "Applying BINLOAD/NetSIO priority patch manually..."
                    if [ -f "src/sio.c" ] && grep 'if (netsio_enabled)' src/sio.c | grep -v BINLOAD | grep -q .; then
                        if sed --version 2>/dev/null | grep -q GNU; then
                            sed -i 's/if (netsio_enabled)/if (netsio_enabled \&\& !BINLOAD_start_binloading)/g' src/sio.c
                        else
                            sed -i '' 's/if (netsio_enabled)/if (netsio_enabled \&\& !BINLOAD_start_binloading)/g' src/sio.c
                        fi
                        echo "✓ BINLOAD/NetSIO priority patch applied manually"
                    elif grep -q 'BINLOAD_start_binloading' src/sio.c; then
                        echo "✓ Patch $patch_name already applied (BINLOAD guard present)"
                    else
                        echo "Error: Patch $patch_name could not be applied and manual fallback failed"
                        exit 1
                    fi
                # For critical patch 0009 (replace netsio_cold_reset with netsio_warm_reset in Atari800_Coldstart), apply manually
                elif [[ "$patch_name" == "0009-replace-netsio-cold-with-warm-reset-in-coldstart.patch" ]]; then
                    echo "Applying netsio_cold_reset -> netsio_warm_reset replacement patch manually..."
                    if [ -f "src/atari.c" ] && grep -q 'netsio_cold_reset' src/atari.c; then
                        # Replace netsio_cold_reset() with netsio_warm_reset() in the NETSIO block
                        # inside Atari800_Coldstart(). A warm reset resets FujiNet-PC's SIO state
                        # without restarting the process, preventing SIGABRT crashes.
                        if sed --version 2>/dev/null | grep -q GNU; then
                            sed -i 's/netsio_cold_reset();/netsio_warm_reset();/g' src/atari.c
                        else
                            sed -i '' 's/netsio_cold_reset();/netsio_warm_reset();/g' src/atari.c
                        fi
                        echo "✓ netsio_cold_reset -> netsio_warm_reset replacement applied manually"
                    elif grep -q 'netsio_warm_reset' src/atari.c; then
                        echo "✓ Patch $patch_name already applied (warm reset present)"
                    else
                        echo "Error: Patch $patch_name could not be applied and manual fallback failed"
                        exit 1
                    fi
                # For critical patch 0003, apply manually
                elif [[ "$patch_name" == "0003-disk-activity-callback-integration.patch" ]]; then
                    echo "Applying disk activity callback patch manually..."

                    # Check if sio.c exists and apply changes
                    if [ -f "src/sio.c" ] && ! grep -q "disk_activity_callback" src/sio.c; then
                        # Create a temporary file with the changes
                        cat > /tmp/sio_patch.txt << 'EOF'
--- a/src/sio.c
+++ b/src/sio.c
@@ -78,6 +78,10 @@
 #include "cassette.h"
 #include "util.h"

+#ifdef LIBATARI800
+extern void (*disk_activity_callback)(int drive, int operation);
+#endif
+
 /* If this is defined, consecutive sectors are read with at most one
    intervening sector. */
 #define CONSECUTIVE_SECTORS_FAST_IO
EOF
                        patch -p1 < /tmp/sio_patch.txt 2>/dev/null || true
                        echo "✓ Manual patch applied for disk activity callback"
                    elif grep -q "disk_activity_callback" src/sio.c; then
                        echo "✓ Patch $patch_name already applied (callback present)"
                    else
                        echo "Error: Patch $patch_name could not be applied and manual fallback failed"
                        exit 1
                    fi
                else
                    echo "Error: Patch $patch_name could not be applied"
                    exit 1
                fi
            fi
        fi
    done
else
    echo "Not a git repository, using 'patch' command"
    for patch in "$PATCHES_DIR"/0*.patch; do
        if [ -f "$patch" ]; then
            patch_name="$(basename "$patch")"

            # Skip patch 0001 - merged into upstream as PR #252 (Oct 13, 2025)
            if [[ "$patch_name" == "0001-libatari800-disk-api.patch" ]]; then
                echo "Skipping patch 0001 - already in upstream (PR #252 merged)"
                continue
            fi

            # Skip patches 0005 and 0006 - accepted into atari800 upstream
            if [[ "$patch_name" == "0005-netsio-so-reuseaddr.patch" ]] || [[ "$patch_name" == "0006-netsio-shutdown.patch" ]]; then
                echo "Skipping patch $patch_name - already in upstream"
                continue
            fi

            echo "Applying patch: $patch_name"
            if patch -p1 --dry-run --force < "$patch" >/dev/null 2>&1; then
                # Use force and no-backup-if-mismatch to avoid prompts
                patch -p1 --force --no-backup-if-mismatch < "$patch" </dev/null || {
                    echo "Error: Failed to apply patch $patch_name"
                    exit 1
                }
            elif patch -p1 --dry-run -R --force < "$patch" >/dev/null 2>&1; then
                echo "✓ Patch $patch_name already applied (skipping)"
            else
                echo "Error: Failed to apply patch $patch_name"
                exit 1
            fi
        fi
    done
fi

echo "All patches applied successfully!"
touch "$PATCH_MARKER"

# Apply inline patches for disk management functions
INLINE_PATCH="$PATCHES_DIR/patch-libatari800-inline.sh"
if [ -f "$INLINE_PATCH" ]; then
    echo "Applying inline patches for disk management functions..."
    bash "$INLINE_PATCH" "$ATARI800_SRC_PATH" || echo "Inline patch may have already been applied"
fi

echo ""
echo "Now build libatari800 with:"
echo "  cd $ATARI800_SRC_PATH"
echo "  ./autogen.sh  # if configure script doesn't exist"
echo "  ./configure --target=libatari800"
echo "  make"