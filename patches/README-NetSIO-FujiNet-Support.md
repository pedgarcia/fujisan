# NetSIO/FujiNet Support Patches for Atari800 Core

This directory contains patches required to enable proper NetSIO/FujiNet support in the Atari800 emulator core. These patches are based on the working implementation from Atari800MacX.

## Problem Statement

The standard Atari800 emulator core has basic NetSIO support (`netsio.c`, `netsio.h`) but lacks proper integration with the SIO (Serial Input/Output) system. This results in:
- FujiNet-PC cannot properly communicate with the emulator
- NetSIO commands are not routed through the SIO system
- Local disk drives do not have priority over FujiNet devices
- Command synchronization issues causing boot failures

## Solution

The Atari800MacX project has an enhanced NetSIO implementation that properly integrates with the SIO system. These patches port those improvements to the standard Atari800 core.

## Required Patches

### 1. netsio-sio-integration.patch
**File**: `src/sio.c`
**Purpose**: Main SIO system integration with NetSIO

**Key Changes**:
- Adds `#ifdef NETSIO` blocks throughout SIO functions
- Implements `NetSIO_PutByte()` and `NetSIO_GetByte()` functions  
- Adds local disk priority logic - local mounted disks take precedence over FujiNet
- Integrates NetSIO command frame handling with existing SIO command processing
- Adds proper synchronization with `netsio_wait_for_sync()` calls
- Implements data transfer chunking for large FujiNet operations

**Critical Features**:
- **Local Disk Priority**: If a local disk (D1:-D8:) is mounted, it handles commands instead of FujiNet
- **Command Routing**: Automatically routes non-local device commands to FujiNet via NetSIO
- **Proper Synchronization**: Uses sync mechanisms to ensure reliable communication
- **Chunked Data Transfer**: Handles large data transfers in 65-byte chunks

### 2. netsio-sio-header.patch  
**File**: `src/sio.h`
**Purpose**: SIO header updates for NetSIO integration

**Key Changes**:
- Adds Mac-specific SIO function `SIO_IsVapi(int diskno)`
- Updates function declarations for NetSIO compatibility

## Installation

1. **Apply patches to clean atari800 source**:
   ```bash
   cd /path/to/atari800-src
   patch -p0 < fujisan/patches/netsio-sio-integration.patch
   patch -p0 < fujisan/patches/netsio-sio-header.patch
   ```

2. **Rebuild libatari800**:
   ```bash
   make clean
   ./configure --target=libatari800
   make -j4
   ```

3. **Rebuild Fujisan**:
   ```bash
   cd fujisan
   make clean 
   make -j4
   ```

## Testing

To test FujiNet support:

1. **Start FujiNet-PC** on port 9997
2. **Enable NetSIO** in Fujisan Media settings
3. **Cold boot** the emulator
4. **Wait for delayed restart** (60 frames / ~1 second after NetSIO init)
5. **Check for FujiNet devices** in Atari directory listings

## Technical Details

### NetSIO Integration Points

1. **SIO_PutByte()**: Enhanced to route commands to NetSIO when appropriate
2. **SIO_GetByte()**: Enhanced to receive data from NetSIO when appropriate  
3. **Command Processing**: Integrated NetSIO command handling with local SIO commands
4. **Device Priority**: Local disks always take priority over FujiNet devices

### Local vs FujiNet Device Logic

```c
// Check if local disk should handle command
if (byte >= 0x31 && byte <= 0x38) {  // D1: through D8:
    int drive = byte - 0x31;
    if (SIO_drive_status[drive] != SIO_OFF && 
        SIO_drive_status[drive] != SIO_NO_DISK) {
        use_local = 1;  // Use local disk
    }
}

if (!use_local) {
    NetSIO_PutByte(byte);  // Route to FujiNet
    return;
}
// Continue with local SIO processing
```

### Synchronization Mechanism

The NetSIO system uses a synchronization mechanism to ensure reliable communication:

```c
netsio_cmd_off_sync();           // Signal command complete with sync request
netsio_wait_for_sync();          // Wait for FujiNet acknowledgment
TransferStatus = SIO_StatusRead; // Continue with status read
```

## Source Attribution

These patches are derived from the Atari800MacX project, which enhanced the standard Atari800 NetSIO implementation for reliable FujiNet communication.

## Version Compatibility

- **Atari800 Core**: 5.2.0 and later
- **NetSIO Protocol**: Compatible with FujiNet-PC
- **Build System**: GNU Autotools with `--enable-netsio` flag

## Debugging

To debug NetSIO issues:

1. **Enable DEBUG**: Compile with `-DDEBUG` flag
2. **Check Logs**: Look for NetSIO debug messages in console output
3. **Monitor Port**: Verify FujiNet-PC is listening on port 9997
4. **Test Connectivity**: Use network tools to verify UDP communication

## Future Improvements

- Enhanced error handling for network failures
- Configurable NetSIO port settings  
- Better integration with Atari800 configuration system
- Support for multiple FujiNet-PC instances