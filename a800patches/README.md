# Atari800 Patches for Fujisan Disk Activity API

This directory contains patches to extend the libatari800 API with disk activity monitoring capabilities required by Fujisan.

## Overview

Fujisan requires real-time disk drive activity information to display authentic Atari 810 disk drive LEDs. The standard libatari800 API does not expose the internal disk I/O state information that the main atari800 emulator uses for its built-in disk activity display.

These patches extend libatari800 with new API functions that provide:
- Real-time disk activity notifications (read/write operations)
- Drive-specific activity information (D1-D8)
- Hardware-level timing that matches the main emulator
- Callback-based and polling-based access methods

## Baseline Version

**Atari800 Version**: b3bde0119fe51c18c2ab7ba2230db8cf8d4f0b88 (master)
**libatari800 Files Modified**: 
- `src/libatari800/libatari800.h` - API declarations
- `src/libatari800/libatari800.c` - API implementation

## Files

- `disk-activity-api.patch` - Main API extension patch
- `examples/` - Example usage code demonstrating the new API
- `test/` - Test programs to verify API functionality
- `apply-patches.sh` - Automated patch application script

## API Functions Added

```c
// Get current disk activity state (polling method)
int libatari800_get_disk_activity(int *drive, int *operation, int *time_remaining);

// Set callback for real-time disk activity events
void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation));

// Get comprehensive drive status information
int libatari800_get_drive_status(int drive_states[8], char filenames[8][256]);
```

## Installation Instructions

### Prerequisites
- Clean atari800 source code at the specified baseline version
- Standard build tools (make, gcc, etc.)
- Git (recommended for patch management)

### Step 1: Apply Patches
```bash
cd /path/to/atari800-src
patch -p1 < /path/to/fujisan/a800patches/disk-activity-api.patch
```

Or use the automated script:
```bash
./a800patches/apply-patches.sh /path/to/atari800-src
```

### Step 2: Rebuild libatari800
```bash
cd /path/to/atari800-src
./autogen.sh
./configure --target=libatari800
make clean
make -j4
```

### Step 3: Verify Installation
```bash
cd a800patches/test
make
./test-disk-activity
```

## Usage Example

```c
#include "libatari800.h"

// Callback method
void disk_activity_callback(int drive, int operation) {
    printf("Drive D%d: %s\n", drive, (operation == 0) ? "READ" : "WRITE");
}

int main() {
    libatari800_init(0, NULL);
    libatari800_set_disk_activity_callback(disk_activity_callback);
    
    // Main emulation loop
    while (running) {
        libatari800_next_frame();
        
        // Or use polling method:
        int drive, operation, time_remaining;
        if (libatari800_get_disk_activity(&drive, &operation, &time_remaining)) {
            printf("Drive D%d active: %s (%d frames remaining)\n", 
                   drive, (operation == 0) ? "READ" : "WRITE", time_remaining);
        }
    }
    
    return 0;
}
```

## Uninstalling Patches

To remove the patches and restore original functionality:

```bash
cd /path/to/atari800-src
patch -R -p1 < /path/to/fujisan/a800patches/disk-activity-api.patch
make clean
make -j4
```

## Contributing Back to Atari800

These patches are designed to be contribution-ready for the main atari800 project:

1. **Clean implementation**: No breaking changes to existing API
2. **Backward compatible**: Existing libatari800 code continues to work
3. **Well documented**: Comprehensive API documentation and examples
4. **Tested**: Includes test suite to verify functionality

To submit to atari800 project:
1. Create a fork of the atari800 repository
2. Apply these patches to your fork
3. Test thoroughly across different platforms
4. Submit a pull request with detailed description

## Troubleshooting

### Patch Application Fails
- Ensure you're using the correct atari800 baseline version
- Check for conflicting local modifications
- Apply patches manually if automated application fails

### Build Errors
- Verify all dependencies are installed
- Check that autogen.sh completed successfully
- Ensure configure was run with `--target=libatari800`

### API Not Working
- Verify patches were applied correctly
- Check that callback is set before starting emulation
- Test with provided example programs

## Support

For issues specific to these patches, please check:
1. Fujisan project documentation
2. Test programs in the `test/` directory
3. Example usage in the `examples/` directory

For general atari800 issues, refer to the main atari800 project documentation.