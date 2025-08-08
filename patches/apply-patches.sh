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
            
            git am "$patch" 2>/dev/null
            if [ $? -ne 0 ]; then
                echo "Warning: Failed to apply patch $patch_name with git am"
                
                # Abort the failed git am
                git am --abort 2>/dev/null || true
                
                # Try with patch command as fallback
                echo "Trying with patch command..."
                if patch -p1 --dry-run < "$patch" >/dev/null 2>&1; then
                    patch -p1 < "$patch"
                    echo "✓ Patch applied successfully with patch command"
                else
                    echo "Warning: Patch $patch_name could not be applied, skipping"
                    continue
                fi
            else
                echo "✓ Patch applied successfully"
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