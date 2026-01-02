# FujiNet-PC: SIO Connection, Timing, and Protocol Analysis

## Overview

FujiNet-PC is the desktop/emulator version of the FujiNet firmware that communicates with Atari emulators over two possible connection types:
1. **Serial SIO**: Direct serial port communication (physical Atari)
2. **NetSIO**: UDP-based network protocol (Altirra emulator)

## 1. PC Platform Architecture

### Core Components

**SioCom Class** (`lib/bus/sio/siocom/fnSioCom.h/cpp`)
- Abstraction layer for SIO communication on PC
- Switches between two port implementations:
  - `SerialSioPort`: Physical serial port communication
  - `NetSioPort`: Network-based UDP communication

**Connection Setup** (from `src/main.cpp`)
```cpp
// Lines 573-576: PC initialization
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

---

## 2. NetSIO Protocol (Network SIO)

### Protocol Overview

NetSIO is a **UDP-based protocol** running on port **9997** that mimics SIO bus signals over the network. It's designed to support Altirra Atari emulator and other PC-based Atari systems.

### Protocol Message Types

**Data Messages:**
- `NETSIO_DATA_BYTE (0x01)`: Single byte transmission
- `NETSIO_DATA_BLOCK (0x02)`: Block of data (up to 512 bytes)
- `NETSIO_DATA_BYTE_SYNC (0x09)`: Single byte with sync request

**Bus Control Signals:**
- `NETSIO_COMMAND_OFF (0x10)`: Command line deasserted
- `NETSIO_COMMAND_ON (0x11)`: Command line asserted
- `NETSIO_COMMAND_OFF_SYNC (0x18)`: Command off with sync request
- `NETSIO_MOTOR_OFF (0x20)`: Motor line off
- `NETSIO_MOTOR_ON (0x21)`: Motor line on
- `NETSIO_PROCEED_OFF (0x30)`: Proceed line off
- `NETSIO_PROCEED_ON (0x31)`: Proceed line on
- `NETSIO_INTERRUPT_OFF (0x40)`: Interrupt line off
- `NETSIO_INTERRUPT_ON (0x41)`: Interrupt line on

**Protocol Control:**
- `NETSIO_SPEED_CHANGE (0x80)`: Baud rate change notification (5 bytes)
- `NETSIO_SYNC_RESPONSE (0x81)`: Late ACK/sync response
- `NETSIO_BUS_IDLE (0x88)`: Bus idle period

**Device Control:**
- `NETSIO_DEVICE_DISCONNECT (0xC0)`: Device disconnecting
- `NETSIO_DEVICE_CONNECT (0xC1)`: Device connecting
- `NETSIO_PING_REQUEST (0xC2)`: Ping (connectivity check)
- `NETSIO_PING_RESPONSE (0xC3)`: Ping response
- `NETSIO_ALIVE_REQUEST (0xC4)`: Keep-alive message
- `NETSIO_ALIVE_RESPONSE (0xC5)`: Keep-alive response
- `NETSIO_CREDIT_STATUS (0xC6)`: Report credit level
- `NETSIO_CREDIT_UPDATE (0xC7)`: Update available credit

**Reset Signals:**
- `NETSIO_WARM_RESET (0xFE)`: Warm reset (soft restart)
- `NETSIO_COLD_RESET (0xFF)`: Cold reset (hard reboot)

### Protocol Packet Structure

**Minimal Packet**: 1-3 bytes
- Command byte + optional parameter bytes

**Data Block Packet**: 1 + up to 512 bytes
- `[0x02] [data...]`

### Located in: 
`/Users/pgarcia/dev/atari/fujinet-firmware/lib/bus/sio/siocom/netsio_proto.h`

---

## 3. Initial Connection and Boot Sequence

### Connection Initialization (NetSioPort::begin)

**Source:** `lib/bus/sio/siocom/netsio.cpp:60-152`

```
1. Check WiFi Connection
   - If no WiFi: suspend for 400-2000ms (based on error count)
   - Max 5 retries before longer suspend

2. Create UDP Socket
   - AF_INET, SOCK_DGRAM, IPPROTO_IP
   - Set non-blocking mode (fcntl O_NONBLOCK)

3. Resolve Host IP
   - Perform DNS lookup of configured host
   - If resolution fails: suspend and retry

4. Connect Socket (UDP)
   - Establishes default destination for UDP packets
   - No actual connection established (UDP is connectionless)

5. Fast Ping Hub
   - Send 2 NETSIO_PING_REQUEST packets (50ms interval)
   - Wait 50ms for response
   - If no response: suspend 1-5 seconds and retry

6. Send Device Connection
   - Send single byte: NETSIO_DEVICE_CONNECT (0xC1)

7. Start Keep-Alive Timer
   - _alive_request = current_time
   - _alive_time = current_time
```

**Timing Constants:**
```cpp
#define ALIVE_RATE_MS       1000    // Send ALIVE_REQUEST every 1 second
#define ALIVE_TIMEOUT_MS    5000    // Declare connection lost if no response in 5 seconds
```

### State After Initialization

When successfully initialized:
- `_initialized = true`
- `_errcount = 0`
- Baud rate set to configured value
- Ready to handle SIO commands

### Boot Configuration (FujiNet-Specific)

**Source:** `lib/device/sio/fuji.h:202`, `lib/device/sio/fuji.cpp:2171`

```cpp
bool boot_config = true;  // Initially enabled
```

During boot, the FujiNet device can provide a boot disk:
```cpp
// lib/bus/sio/sio.cpp:300-310
if (tempFrame.device == SIO_DEVICEID_DISK && 
    _fujiDev != nullptr && 
    _fujiDev->boot_config)
{
    _activeDev = _fujiDev->bootdisk();
    // Boot configuration disk is served instead of real disks
}
```

**Boot Config is Disabled When:**
- Status/wait command issued while `status_wait_enabled = true`
- Explicitly set via SIO command 0xD9 `sio_set_boot_config()`
- Manually disabled by user

---

## 4. Reconnection Behavior

### Keep-Alive Mechanism

**Source:** `lib/bus/sio/siocom/netsio.cpp:300-334`

The NetSIO implementation maintains connection health through periodic keep-alive:

```
Every 1 second (ALIVE_RATE_MS):
  
  IF 5 seconds (ALIVE_TIMEOUT_MS) passed since LAST RESPONSE:
    - Connection declared LOST
    - Attempt 2 PING requests (1 second interval, 2 second timeout)
    - If PING succeeds:
      * Call end() to close socket
      * Call begin() to reconnect (full reconnection)
    - If PING fails:
      * Suspend for 5 seconds
      * Set error counter
      * No reconnection attempt
  
  ELSE IF no data received for >= 1 second:
    - Send NETSIO_ALIVE_REQUEST (0xC4) to hub
```

### Reconnection Flow

1. **Detect Connection Loss** (5 second timeout)
2. **Ping Attempt** (to confirm loss)
3. **Socket Cleanup** (if ping succeeds)
4. **Reinitialize** (full begin() call)

### Error Handling and Backoff

**Suspension Logic** (`lib/bus/sio/siocom/netsio.cpp:75, 88`):
```cpp
int suspend_ms = _errcount < 5 ? 400 : 2000;  // Initial attempts: 400ms
int suspend_ms = _errcount < 5 ? 1000 : 5000; // Connection failures: 1-5 seconds

// After suspension expires, resume_test() triggers new begin() call
```

### State After Reconnection vs Initial Connection

**Key Difference: boot_config state persists**

```cpp
// lib/bus/sio/sio.cpp:300-310
// boot_config is only disabled when:
// 1. STATUS command issued during boot
// 2. Explicit SIO command 0xD9
// 3. Failed boot attempt

// During reconnection, boot_config remains in its previous state
// This means:
// - If boot was completed before disconnection: boot_config = false
// - If disconnected before boot completed: boot_config remains true
```

**State Preservation Across Reconnections:**
- Device configuration: **preserved** (stored in flash/config)
- Boot config flag: **preserved** (stays as-is)
- Command processing state: **reset** (new connection = fresh state)
- Baud rate: **reset** (renegotiated)
- Sync state: **reset** (fresh command frame)

---

## 5. SIO Command Frame Processing

### Command Reception Flow

**Source:** `lib/bus/sio/sio.cpp:250-330`

```
Main Loop: fnSioCom.poll(1)
  ↓
While command asserted:
  ↓
Read 4-byte command frame:
  - Device ID (1 byte)
  - Command (1 byte)
  - Aux1 (1 byte)
  - Aux2 (1 byte)
  ↓
Calculate and verify checksum
  ↓
[IF boot_config enabled AND device is disk (0x31)]
  Route to boot disk (_fujiDev->bootdisk())
ELSE
  Find device in device chain
  Route to appropriate handler
  ↓
Device processes command and sends:
  - ACK (0x41) or NAK (0x4E)
  - Optional data
  - COMPLETE (0x43) or ERROR (0x45)
```

### Cold Reset Handling

**Source:** `lib/bus/sio/siocom/netsio.cpp:423-428`

```cpp
case NETSIO_COLD_RESET:
    // emulator cold reset, do fujinet restart
#ifndef DEBUG_NO_REBOOT
    fnSystem.reboot();
#endif
    break;
```

When an Atari cold boot is detected (via `NETSIO_COLD_RESET`):
- **PC action**: Full system reboot
- **FujiNet state**: All devices reset
- **Boot config**: Reset to default (true) per `lib/device/sio/fuji.cpp:2171`

---

## 6. Timing Expectations and Critical Delays

### T4 and T5 Delays

**Source:** `lib/bus/sio/sio.h:23-24`

```cpp
#define DELAY_T4 850    // Delay before accepting data frame
#define DELAY_T5 250    // Delay after ACK/NAK before COMPLETE
```

These match standard SIO timing:
- **T4** (850 µs): Time Atari waits for proceed assertion
- **T5** (250 µs): Inter-byte delay in device response

### Baud Rate Changes

**Default:** 19200 baud

**High-Speed Index Configuration:**
```cpp
#define SIO_HISPEED_INDEX 0x06  // ~67,431 baud (configurable)
```

Baud changes are negotiated via SIO command 0xEB (`sio_set_baudrate`).

NetSIO announces peer baud rate:
```cpp
case NETSIO_SPEED_CHANGE:
    if (received >= 5)
    {
        _baud_peer = rxbuf[1] | (rxbuf[2] << 8) | (rxbuf[3] << 16) | (rxbuf[4] << 24);
    }
```

### Sync Response (Late ACK)

**Purpose:** Optimize timing for high-speed SIO transfers

NetSIO supports "sync response" - ACK sent immediately after data received rather than before processing:

```cpp
// lib/bus/sio/siocom/netsio.cpp:730-737
if (_sync_request_num >= 0 && _sync_write_size >= 0)
{
    // handle pending sync request
    // send late ACK byte
    send_sync_response(NETSIO_ACK_SYNC, _sync_ack_byte, _sync_write_size);
}
```

This allows the PC to process data while the Atari can proceed to the next operation.

---

## 7. Network Protocol Details

### Flow Control (Credit System)

**Source:** `lib/bus/sio/siocom/netsio.cpp:569-590`

NetSIO implements a credit-based flow control system:

```
Initial credit: 3 units

For each write operation:
  1. Check if needed_bytes > available_credit
  2. If not enough credit:
     - Send NETSIO_CREDIT_STATUS with current credit
     - Wait for NETSIO_CREDIT_UPDATE from hub
     - Repeat until sufficient credit
  3. Deduct credit for transmission
```

This prevents buffer overflows when the hub has limited receive capacity.

### Socket Operations

**Timeout Management:**
```cpp
// lib/bus/sio/siocom/netsio.cpp:448-492 (wait_sock_readable)
// lib/bus/sio/siocom/netsio.cpp:494-537 (wait_sock_writable)

// Uses select() with configurable timeout
// Handles EINTR properly (interrupted system calls)
```

### Data Reception Buffering

**Source:** `lib/bus/sio/siocom/netsio.cpp:22-25`

```cpp
uint8_t _rxbuf[1024];  // 1KB receive buffer
int _rxhead;           // Write pointer
int _rxtail;           // Read pointer
bool _rxfull;          // Overflow flag
```

Circular buffer for incoming data:
- Prevents packet loss during command processing
- Detects overflows (prints debug message)
- Can be flushed on command line assertion

---

## 8. PC-Specific Device Connection Lifecycle

### Scenario 1: Initial Atari Boot (Cold Start)

```
Time T=0: Atari power on
  ↓
FujiNet receives NETSIO_COLD_RESET (0xFF)
  ↓
FujiNet reboots (full reinitialization)
  ↓
boot_config flag reset to true
  ↓
Atari sends disk read command to device 0x31
  ↓
SIO router sees boot_config=true
  ↓
Serves boot disk instead of real disk
  ↓
User completes boot/config
  ↓
Atari sends STATUS to boot disk
  ↓
boot_config set to false
  ↓
Subsequent disk commands go to real mounted disks
```

### Scenario 2: Reconnection During Runtime

```
Time T=0: FujiNet connected, boot_config=false
  ↓
Network interruption detected
  ↓
ALIVE_TIMEOUT_MS (5 seconds) expires
  ↓
Attempt 2 pings
  ↓
PING succeeds
  ↓
Call end() → close socket, send DISCONNECT
  ↓
Call begin() → new socket, send CONNECT
  ↓
boot_config remains false (persisted)
  ↓
Disk commands continue to real disks (no boot)
```

### Scenario 3: Power Loss / Unexpected Disconnect

```
Time T=0: Network connection drops (no COLD_RESET)
  ↓
Keep-alive messages fail for 5 seconds
  ↓
Connection loss detected
  ↓
Ping attempt → fails (no response)
  ↓
Suspend for 5 seconds
  ↓
Backoff timer expires
  ↓
Retry begin()
  ↓
If Atari is still powered: responds to commands
If Atari is powered off: timeouts on SIO reads
```

---

## 9. Configuration and Initial Setup

### Configuration Sources

**Source:** `lib/bus/sio/sio.cpp:572-585` (PC setup)

```cpp
fnSioCom.set_serial_port(
    Config.get_serial_port().c_str(),      // e.g., "/dev/ttyUSB0"
    Config.get_serial_command(),            // GPIO pin for command line
    Config.get_serial_proceed()              // GPIO pin for proceed line
);

fnSioCom.set_netsio_host(
    Config.get_boip_host().c_str(),         // e.g., "192.168.1.100"
    Config.get_boip_port()                  // Usually 9997
);

fnSioCom.set_sio_mode(
    Config.get_boip_enabled() ?             // Use NetSIO if true, Serial if false
    SioCom::sio_mode::NETSIO :
    SioCom::sio_mode::SERIAL
);

fnSioCom.begin(_sioBaud);                  // Initialize connection
```

### Boot Configuration Defaults

**Source:** `lib/device/sio/fuji.cpp:2171`

```cpp
boot_config = Config.get_general_config_enabled();  // Read from persistent config
```

Initial state is configurable, but defaults to true (boot config enabled).

---

## 10. Summary: Key Insights for Fujisan Integration

### Connection Management
- PC platform uses abstraction layer (SioCom) for serial/network switching
- NetSIO is UDP-based, not TCP - connectionless but with keep-alive heartbeats
- Connection loss detected after 5-second timeout with keep-alive failure
- Automatic reconnection with exponential backoff (400ms → 1-5 seconds)

### Timing Expectations
- **Initial boot**: Expect ~1-2 seconds for NetSIO initialization + DNS + ping
- **Keep-alive heartbeat**: 1-second intervals after initial connection
- **Reconnection**: Can take 5+ seconds if network is slow/unreliable
- **Cold reset**: Full FujiNet reboot, boot_config reset to enabled

### State Persistence
- **Across reconnections**: Device configuration, boot_config, HSIO settings persist
- **Across warm resets**: State not reset (different from cold boot)
- **Across cold resets**: Everything reset (via fnSystem.reboot())

### Boot Config Behavior
- **Initial state**: true (enabled) - boot disk served on first disk command
- **Disabled when**: STATUS command issued, or explicit SIO command
- **Persists through**: Reconnections, temporary network loss
- **Resets on**: Cold boot (NETSIO_COLD_RESET)

### Critical Differences from ESP32
- No FreeRTOS - uses custom task manager (`fnTaskManager.h/cpp`)
- Uses Mongoose for HTTP service (not ESP32's HTTP server)
- Network stacks differ (Linux/macOS/Windows vs ESP-IDF)
- File I/O uses local filesystem, not SPIFFS/LittleFS
- No PSRAM considerations

