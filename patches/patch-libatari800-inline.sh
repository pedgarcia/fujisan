#!/bin/bash
# Inline patch for libatari800 to add missing disk management functions

ATARI800_SRC="$1"

if [ -z "$ATARI800_SRC" ]; then
    echo "Usage: $0 <atari800-src-dir>"
    exit 1
fi

echo "Patching libatari800 in: $ATARI800_SRC"

# Check if already patched
if grep -q "libatari800_unmount_disk" "$ATARI800_SRC/src/libatari800/api.c" 2>/dev/null; then
    echo "Already patched - skipping"
    exit 0
fi

echo "Adding disk management functions..."

# Create temporary file with the new functions
cat > /tmp/disk_functions.c << 'EOF'

/* Disk activity callback function pointer */
void (*disk_activity_callback)(int drive, int operation) = NULL;

/* Disk management functions */
int libatari800_mount_disk(int drive_num, const char *filename, int read_only)
{
	if (drive_num < 1 || drive_num > 8 || filename == NULL)
		return FALSE;
	
	/* Use SIO_Mount with read_only flag properly mapped */
	if (SIO_Mount(drive_num, filename, read_only ? TRUE : FALSE))
		return TRUE;
	
	return FALSE;
}

void libatari800_unmount_disk(int drive_num)
{
	if (drive_num < 1 || drive_num > 8)
		return;
	
	SIO_Dismount(drive_num);
}

void libatari800_disable_drive(int drive_num)
{
	if (drive_num < 1 || drive_num > 8)
		return;
	
	SIO_DisableDrive(drive_num);
}

void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation))
{
	disk_activity_callback = callback;
}

int libatari800_get_sio_patch_enabled(void)
{
	return ESC_enable_sio_patch;
}

int libatari800_set_sio_patch_enabled(int enabled)
{
	int prev = ESC_enable_sio_patch;
	ESC_enable_sio_patch = enabled;
	return prev;
}
EOF

# 1. Add includes to api.c if not already present
if ! grep -q "#include \"../sio.h\"" "$ATARI800_SRC/src/libatari800/api.c"; then
    # Find the line with #include "atari.h" and add our includes after it
    sed -i.bak '/#include "atari.h"/a\
#include "libatari800.h"\
#include "../sio.h"\
#include "../esc.h"\
' "$ATARI800_SRC/src/libatari800/api.c"
fi

# 2. Add functions to api.c (before the final vim comment)
# Remove the vim comment temporarily, add our functions, then add it back
sed -i.bak '/^\/\*$/,/^\*\/$/d' "$ATARI800_SRC/src/libatari800/api.c"
cat /tmp/disk_functions.c >> "$ATARI800_SRC/src/libatari800/api.c"
echo "" >> "$ATARI800_SRC/src/libatari800/api.c"
echo "/*" >> "$ATARI800_SRC/src/libatari800/api.c"
echo "vim:ts=4:sw=4:" >> "$ATARI800_SRC/src/libatari800/api.c"
echo "*/" >> "$ATARI800_SRC/src/libatari800/api.c"

# 3. Add declarations to libatari800.h
if ! grep -q "libatari800_unmount_disk" "$ATARI800_SRC/src/libatari800/libatari800.h"; then
    sed -i.bak '/void libatari800_exit();/a\
\
/* Disk management functions */\
int libatari800_mount_disk(int drive_num, const char *filename, int read_only);\
void libatari800_unmount_disk(int drive_num);\
void libatari800_disable_drive(int drive_num);\
void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation));\
\
/* SIO patch control functions */\
int libatari800_get_sio_patch_enabled(void);\
int libatari800_set_sio_patch_enabled(int enabled);' "$ATARI800_SRC/src/libatari800/libatari800.h"
fi

# 4. Add disk activity callback hooks to sio.c
if ! grep -q "disk_activity_callback" "$ATARI800_SRC/src/sio.c"; then
    # Add extern declaration after IMAGE_TYPE_VAPI - ensure proper newlines
    sed -i.bak '/#define IMAGE_TYPE_VAPI 3/a\
\
#ifdef LIBATARI800\
extern void (*disk_activity_callback)(int drive, int operation);\
#endif\
' "$ATARI800_SRC/src/sio.c"

    # Add callback hooks - we need to be careful with the patterns
    # For ReadSector
    sed -i.bak '/SIO_last_drive = unit + 1;/{
        a\
#ifdef LIBATARI800\
	/* Call disk activity callback for real-time LED updates */\
	if (disk_activity_callback) {\
		disk_activity_callback(SIO_last_drive, SIO_LAST_READ);\
	}\
#endif
    }' "$ATARI800_SRC/src/sio.c"

    # For WriteSector  
    sed -i.bak '/SIO_last_op = SIO_LAST_WRITE;/{
        N
        N
        /SIO_last_drive = unit + 1;/a\
#ifdef LIBATARI800\
	/* Call disk activity callback for real-time LED updates */\
	if (disk_activity_callback) {\
		disk_activity_callback(SIO_last_drive, SIO_LAST_WRITE);\
	}\
#endif
    }' "$ATARI800_SRC/src/sio.c"
fi

# Clean up
rm -f /tmp/disk_functions.c
rm -f "$ATARI800_SRC/src/libatari800/"*.bak
rm -f "$ATARI800_SRC/src/"*.bak

echo "✓ Patching complete"