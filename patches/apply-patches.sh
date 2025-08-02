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
            echo "Applying patch: $(basename "$patch")"
            git am "$patch"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to apply patch $(basename "$patch")"
                echo "Git am exit code: $?"
                
                # Try to show more details about the failure
                git am --show-current-patch 2>/dev/null || true
                
                echo ""
                echo "You may need to run 'git am --abort' to reset"
                echo "Or try applying with 'patch -p1 < $patch' instead"
                exit 1
            fi
            
            # Verify the patch was actually applied by checking if files were modified
            if ! git diff --quiet HEAD~1 HEAD 2>/dev/null; then
                echo "✓ Patch applied successfully"
            else
                echo "Warning: git am reported success but no changes were detected"
                echo "This might indicate the patch was already applied or failed silently"
            fi
        fi
    done
else
    echo "Not a git repository, using 'patch' command"
    for patch in "$PATCHES_DIR"/0*.patch; do
        if [ -f "$patch" ]; then
            echo "Applying patch: $(basename "$patch")"
            patch -p1 < "$patch" || {
                echo "Error: Failed to apply patch $(basename "$patch")"
                exit 1
            }
        fi
    done
fi

echo "All patches applied successfully!"
echo ""
echo "Now build libatari800 with:"
echo "  cd $ATARI800_SRC_PATH"
echo "  ./autogen.sh  # if configure script doesn't exist"
echo "  ./configure --target=libatari800"
echo "  make"