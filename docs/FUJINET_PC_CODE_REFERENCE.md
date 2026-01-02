# FujiNet-PC: Code Reference Guide

Quick reference to key files and functions for understanding FujiNet-PC SIO handling.

## File Locations

All paths relative to: `/Users/pgarcia/dev/atari/fujinet-firmware/`

## Core SIO Communication

### SioCom (Abstraction Layer)
- **Header**: `lib/bus/sio/siocom/fnSioCom.h`
- **Implementation**: `lib/bus/sio/siocom/fnSioCom.cpp`
- **Key Methods**:
  - `begin(int baud)` - Initialize connection
  - `poll(int ms)` - Check for SIO activity
  - `set_sio_mode(sio_mode mode)` - Switch between SERIAL/NETSIO
  - `command_asserted()` - Check if Atari issued command

### NetSIO Protocol Handler
- **Header**: `lib/bus/sio/siocom/netsio.h`
- **Implementation**: `lib/bus/sio/siocom/netsio.cpp` (LARGE - 900+ lines)
- **Protocol Definition**: `lib/bus/sio/siocom/netsio_proto.h`

**Key Functions in netsio.cpp**:
- `NetSioPort::begin()` (L60-152) - Connection initialization
- `NetSioPort::keep_alive()` (L300-334) - Keep-alive heartbeat
- `NetSioPort::handle_netsio()` (L337-438) - Receive and process messages
- `NetSioPort::poll()` (L168-178) - Main polling entry point
- `NetSioPort::read()` (L709-720) - Read single byte with timeout
- `NetSioPort::write()` (L764-829) - Write data with credit control

**Key Constants**:
```cpp
#define ALIVE_RATE_MS       1000    // Keep-alive send rate
#define ALIVE_TIMEOUT_MS    5000    // Connection lost threshold
#define NETSIO_PORT         9997    // UDP port number
```

## SIO Bus Implementation

### Main SIO Bus Handler
- **Header**: `lib/bus/sio/sio.h`
- **Implementation**: `lib/bus/sio/sio.cpp` (650+ lines)

**Key Functions**:
- `systemBus::setup()` (L533-586) - Initialize SIO bus
  - PC-specific: L572-585 (serial + NetSIO configuration)
  - Sets up SioCom with config values
  
- `systemBus::poll()` - Main event loop (handles command frames)
  - Command reception
  - Boot disk routing logic
  - Device dispatch

**Boot Configuration Logic** (L290-310):
```cpp
if (tempFrame.device == SIO_DEVICEID_DISK && 
    _fujiDev != nullptr && 
    _fujiDev->boot_config)
{
    _activeDev = _fujiDev->bootdisk();
    // ... serves boot config instead of real disk
}
```

### SIO Timing Constants
- `DELAY_T4` (850 µs) - Data frame setup time
- `DELAY_T5` (250 µs) - Response inter-byte delay
- `SIO_STANDARD_BAUDRATE` (19200) - Default baud rate

## FujiNet Device Implementation

### FujiNet Control Device
- **Header**: `lib/device/sio/fuji.h`
- **Implementation**: `lib/device/sio/fuji.cpp` (2700+ lines)

**Key Members**:
- `boot_config` (L202) - Boot configuration disk flag
- `_bootDisk` (L98) - Virtual boot disk device
- `bootdisk()` (L206) - Get boot disk instance

**Key Methods**:
- `setup()` - Initialize FujiNet device
- `sio_process()` - Handle SIO commands
- `sio_set_boot_config()` (L536-539) - SIO command 0xD9
- `sio_reset_fujinet()` (L119) - SIO command 0xFF (reset)

**Boot Config Initialization** (L2171):
```cpp
boot_config = Config.get_general_config_enabled();
```

## Main Application Setup

### Application Entry Point
- **File**: `src/main.cpp`
- **PC-Specific Setup** (L112-200):
  - Signal handlers (SIGINT, SIGTERM, SIGHUP, SIGUSR1)
  - Windows Winsock initialization
  
- **SIO Configuration** (L278-289):
  - Sets up fnSioCom as SYSTEM_BUS UART
  - Calls `SIO.setup()`

**PC-Specific Initialization** (L166-211):
```cpp
#else
// !ESP_PLATFORM
// Signal handlers for graceful shutdown/restart
// Winsock initialization for Windows
```

## Configuration System

### Configuration Files
- **Main Config**: `lib/config/fnConfig.h/cpp`
- **Serial Config**: `Config.get_serial_port()`, `get_serial_command()`, `get_serial_proceed()`
- **NetSIO Config**: `Config.get_boip_host()`, `get_boip_port()`, `get_boip_enabled()`
- **Boot Config**: `Config.get_general_config_enabled()`

### Configuration in SIO Setup
From `lib/bus/sio/sio.cpp:573-576`:
```cpp
fnSioCom.set_serial_port(Config.get_serial_port().c_str(), 
                         Config.get_serial_command(), 
                         Config.get_serial_proceed());
fnSioCom.set_netsio_host(Config.get_boip_host().c_str(), 
                         Config.get_boip_port());
fnSioCom.set_sio_mode(Config.get_boip_enabled() ? 
                      SioCom::sio_mode::NETSIO : 
                      SioCom::sio_mode::SERIAL);
fnSioCom.begin(_sioBaud);
```

## Protocol Message Handling

### Incoming Message Types (netsio.cpp:356-432)

**Data Reception**:
- `NETSIO_DATA_BYTE` (0x01) - Single byte
- `NETSIO_DATA_BLOCK` (0x02) - Block (up to 512 bytes)
- `NETSIO_DATA_BYTE_SYNC` (0x09) - Byte with sync

**Bus Control**:
- `NETSIO_COMMAND_OFF` / `NETSIO_COMMAND_ON` (0x10/0x11)
- `NETSIO_MOTOR_OFF` / `NETSIO_MOTOR_ON` (0x20/0x21)
- `NETSIO_PROCEED_OFF` / `NETSIO_PROCEED_ON` (0x30/0x31)
- `NETSIO_INTERRUPT_OFF` / `NETSIO_INTERRUPT_ON` (0x40/0x41)

**Connection Management**:
- `NETSIO_PING_REQUEST` / `NETSIO_PING_RESPONSE` (0xC2/0xC3)
- `NETSIO_ALIVE_REQUEST` / `NETSIO_ALIVE_RESPONSE` (0xC4/0xC5)
- `NETSIO_DEVICE_CONNECT` / `NETSIO_DEVICE_DISCONNECT` (0xC1/0xC0)

**Reset Signals**:
- `NETSIO_COLD_RESET` (0xFF) - Full reboot (L423-428)
- `NETSIO_WARM_RESET` (0xFE) - Soft restart

## Cross-Platform Differences

### ESP32 vs PC
- **ESP32**: Uses FreeRTOS, hardware pins, SD card via SPIFFS
- **PC**: Uses task manager, serial/network ports, local filesystem

**#ifdef Guards**:
- `#ifdef ESP_PLATFORM` - ESP32 specific code
- `#else` - PC specific code (serial port, Mongoose HTTP, etc.)
- `#ifdef BUILD_ATARI` - Atari-specific device initialization

### Conditional Compilation

**In src/main.cpp**:
- L1-11: Platform-specific includes
- L62-98: PC-specific setup (signal handlers, Winsock)
- L166-211: PC initialization block
- L276-278: PC UART setup (fnSioCom instead of fnUartBUS)

## Debugging and Monitoring

### Debug Output

**Key Debug Macros**:
- `Debug_printf()` - Printf-style output
- `Debug_println()` - Line output

**Verbose SIO Mode**:
In `lib/bus/sio/sio.cpp`:
- `#ifdef VERBOSE_SIO` - Packet-level logging
- Shows command frames, checksums, data blocks

**NetSIO Debugging**:
In `lib/bus/sio/siocom/netsio.cpp`:
- L349-354: Received packet hex dump
- L222: Ping response time
- Line 311: Connection loss detection
- Line 625: Baud rate changes

### Connection State Monitoring

**From netsio.h**:
- `_initialized` - Connection active
- `_command_asserted` - CMD line status
- `_motor_asserted` - MTR line status
- `_errcount` - Error counter
- `_alive_time` - Last message received
- `_alive_request` - Last keep-alive sent

## Common Operations

### Reading SIO Command Frame

From `lib/bus/sio/sio.cpp`:
1. Check `fnSioCom.command_asserted()`
2. Read 4 bytes: device ID, command, aux1, aux2
3. Calculate/verify checksum
4. Route to appropriate device
5. Device responds with ACK/NAK + data + COMPLETE/ERROR

### Switching Connection Mode

From `lib/bus/sio/siocom/fnSioCom.cpp:236-247`:
```cpp
void SioCom::set_sio_mode(sio_mode mode)
{
    _sio_mode = mode;
    switch(mode)
    {
    case sio_mode::NETSIO:
        _sioPort = &_netSio;
        break;
    default:
        _sioPort = &_serialSio;
    }
}
```

### Resetting Connection

From `lib/bus/sio/siocom/fnSioCom.cpp:250-256`:
```cpp
void SioCom::reset_sio_port(sio_mode mode)
{
    uint32_t baud = get_baudrate();
    end();
    set_sio_mode(mode);
    begin(baud);
}
```

## Important Notes for Fujisan

1. **Boot Config**: Check `_fujiDev->boot_config` flag (L300 in sio.cpp)
2. **Connection Type**: Use `fnSioCom.get_sio_mode()` to determine serial vs NetSIO
3. **Keep-Alive**: NetSIO sends ALIVE_REQUEST every 1 second after connection
4. **Cold Reset**: FujiNet calls `fnSystem.reboot()` on COLD_RESET
5. **Reconnection**: Automatic with backoff; takes 5+ seconds if network is down
6. **State Persistence**: Device config persists across reconnections; boot_config persists unless explicitly changed

