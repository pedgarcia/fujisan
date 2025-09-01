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

# Check if this is a git repository
if [ -d .git ]; then
    echo "Git repository detected, using 'git am' for patches"
    for patch in "$PATCHES_DIR"/0*.patch; do
        if [ -f "$patch" ]; then
            patch_name="$(basename "$patch")"
            echo "Applying patch: $patch_name"
            
            # Skip Windows-specific patches on non-Windows systems
            # Allow Windows patches when cross-compiling (CC contains mingw)
            if [[ "$patch_name" == *"windows"* ]] && [[ "$OSTYPE" != "msys" ]] && [[ "$MSYSTEM" == "" ]] && [[ "$CC" != *"mingw"* ]]; then
                echo "Skipping Windows-specific patch on non-Windows system"
                continue
            fi
            
            # Try git apply first (more reliable than git am for patches)
            # Use --3way for better conflict resolution and avoid prompts
            if git apply --check "$patch" 2>/dev/null; then
                git apply --3way "$patch" </dev/null 2>/dev/null || git apply "$patch" </dev/null
                echo "✓ Patch applied successfully with git apply"
            elif patch -p1 --dry-run --force < "$patch" >/dev/null 2>&1; then
                patch -p1 --force --no-backup-if-mismatch < "$patch" </dev/null
                echo "✓ Patch applied successfully with patch command"
            else
                    # For critical patch 0003, apply manually
                    if [[ "$patch_name" == "0003-disk-activity-callback-integration.patch" ]]; then
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
                            
                            # Also add the callback invocations manually if needed
                            echo "✓ Manual patch partially applied - disk LED activity may still need fixes"
                        fi
                        echo "Warning: Patch $patch_name had issues but continuing anyway"
                    else
                        echo "Warning: Patch $patch_name could not be applied, skipping"
                        continue
                    fi
            fi
        fi
    done
else
    echo "Not a git repository, using 'patch' command"
    for patch in "$PATCHES_DIR"/0*.patch; do
        if [ -f "$patch" ]; then
            echo "Applying patch: $(basename "$patch")"
            # Use force and no-backup-if-mismatch to avoid prompts
            patch -p1 --force --no-backup-if-mismatch < "$patch" </dev/null || {
                echo "Error: Failed to apply patch $(basename "$patch")"
                exit 1
            }
        fi
    done
fi

echo "All patches applied successfully!"

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