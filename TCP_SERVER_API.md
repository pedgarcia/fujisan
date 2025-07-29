# Fujisan TCP Server API Documentation

The Fujisan TCP Server provides a comprehensive JSON-based API for remote control of the Atari emulator. This enables IDE integration, automated testing workflows, and programmatic control of all emulator functions.

## Table of Contents

- [Getting Started](#getting-started)
- [Connection & Protocol](#connection--protocol)
- [Command Categories](#command-categories)
  - [Media Commands](#media-commands)
  - [System Commands](#system-commands)
  - [Input Commands](#input-commands)
  - [Debug Commands](#debug-commands)
  - [Config Commands](#config-commands)
  - [Status Commands](#status-commands)
- [Event System](#event-system)
- [Error Handling](#error-handling)
- [Integration Examples](#integration-examples)

## Getting Started

### Enabling the TCP Server

1. Launch Fujisan
2. Go to **Tools** → **TCP Server** (or press the menu item)
3. Server starts on `localhost:8080` by default
4. Server status appears in the status bar

### Quick Test

```bash
# Test connection - should return welcome message
nc localhost 8080

# Test with curl (using HTTP-like syntax but it's raw TCP)
echo '{"command": "status.get_state"}' | nc localhost 8080
```

## Connection & Protocol

### Protocol Details

- **Transport**: Raw TCP on localhost:8080
- **Format**: Newline-delimited JSON messages
- **Security**: Localhost-only for security
- **Multi-client**: Supports multiple simultaneous connections

### Message Format

**Request:**
```json
{
  "command": "category.action",
  "id": "optional-request-id",
  "params": {
    "parameter1": "value1",
    "parameter2": "value2"
  }
}
```

**Response:**
```json
{
  "type": "response",
  "status": "success|error",
  "id": "matching-request-id",
  "result": { /* response data */ },
  "error": "error message if status=error"
}
```

**Event:**
```json
{
  "type": "event",
  "event": "event_name",
  "data": { /* event data */ }
}
```

## Command Categories

### Media Commands

Control disk drives, cartridges, and executable loading.

#### `media.insert_disk`

Insert a disk image into a drive (D1: through D8:).

```bash
curl -X POST localhost:8080 --data-raw '{
  "command": "media.insert_disk",
  "id": "insert-1",
  "params": {
    "drive": 1,
    "path": "/path/to/disk.atr"
  }
}'
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "id": "insert-1",
  "result": {
    "drive": 1,
    "path": "/path/to/disk.atr",
    "mounted": true
  }
}
```

#### `media.eject_disk`

Eject disk from specified drive.

```bash
echo '{"command": "media.eject_disk", "params": {"drive": 1}}' | nc localhost 8080
```

#### `media.enable_drive`

Enable a disk drive (turn on).

```bash
echo '{"command": "media.enable_drive", "params": {"drive": 1}}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "drive": 1,
    "enabled": true
  }
}
```

#### `media.disable_drive`

Disable a disk drive (turn off).

```bash
echo '{"command": "media.disable_drive", "params": {"drive": 1}}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response", 
  "status": "success",
  "result": {
    "drive": 1,
    "enabled": false
  }
}
```

#### `media.insert_cartridge`

Insert a cartridge file (.car format). Updates GUI cartridge widget visual state.

```bash
echo '{
  "command": "media.insert_cartridge",
  "params": {"path": "/path/to/cartridge.car"}
}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response",
  "status": "success", 
  "result": {
    "path": "/path/to/cartridge.car",
    "loaded": true
  }
}
```

#### `media.eject_cartridge`

Eject the currently inserted cartridge. Updates GUI cartridge widget visual state.

```bash
echo '{"command": "media.eject_cartridge"}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "ejected": true
  }
}
```

#### `media.load_xex`

Load and execute an Atari executable file (.xex format). Program starts running immediately.

```bash
echo '{
  "command": "media.load_xex", 
  "params": {"path": "/path/to/program.xex"}
}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "path": "/path/to/program.xex", 
    "loaded": true
  }
}
```

**Note:** To clear XEX programs from memory, use `system.cold_boot`.

### System Commands

Control emulator execution and system state.

#### `system.cold_boot`

Perform cold boot (complete system restart).

```bash
echo '{"command": "system.cold_boot"}' | nc localhost 8080
```

#### `system.warm_boot`

Perform warm boot (soft restart).

```bash
echo '{"command": "system.warm_boot"}' | nc localhost 8080
```

#### `system.pause` / `system.resume`

Pause or resume emulation.

```bash
# Pause
echo '{"command": "system.pause"}' | nc localhost 8080

# Resume
echo '{"command": "system.resume"}' | nc localhost 8080
```

#### `system.set_speed`

Set emulation speed (10-1000%).

```bash
echo '{
  "command": "system.set_speed",
  "params": {"percentage": 200}
}' | nc localhost 8080
```

### Input Commands

Send keyboard input and control keys to the emulator.

#### `input.send_text`

Send a text string to the emulator.

```bash
echo '{
  "command": "input.send_text",
  "params": {"text": "LOAD \"D:HELLO.BAS\"\nRUN\n"}
}' | nc localhost 8080
```

#### `input.send_key`

Send individual keys.

```bash
# Send Enter key
echo '{
  "command": "input.send_key",
  "params": {"key": "RETURN"}
}' | nc localhost 8080

# Send Escape
echo '{
  "command": "input.send_key",
  "params": {"key": "ESC"}
}' | nc localhost 8080
```

**Supported keys:** `RETURN`, `ENTER`, `SPACE`, `TAB`, `ESC`, `ESCAPE`, `BACKSPACE`, or any single character.

#### `input.console_key`

Send Atari console keys.

```bash
# Press START
echo '{
  "command": "input.console_key",
  "params": {"key": "START"}
}' | nc localhost 8080

# Press SELECT
echo '{
  "command": "input.console_key",
  "params": {"key": "SELECT"}
}' | nc localhost 8080
```

**Supported console keys:** `START`, `SELECT`, `OPTION`, `RESET`

#### `input.function_key`

Send function keys F1-F4.

```bash
echo '{
  "command": "input.function_key",
  "params": {"key": "F1"}
}' | nc localhost 8080
```

#### `input.break`

Send break signal (Ctrl+C equivalent).

```bash
echo '{"command": "input.break"}' | nc localhost 8080
```

### Debug Commands

Control debugging features, breakpoints, and memory access.

#### `debug.get_registers`

Get current CPU register values.

```bash
echo '{"command": "debug.get_registers"}' | nc localhost 8080
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "pc": "$E477",
    "a": "$00",
    "x": "$FF",
    "y": "$00",
    "s": "$FB",
    "p": "$34"
  }
}
```

#### `debug.read_memory`

Read memory from specified address.

```bash
echo '{
  "command": "debug.read_memory",
  "params": {"address": 1536, "length": 16}
}' | nc localhost 8080
```

**Response:**
```json
{
  "result": {
    "address": "$0600",
    "length": 16,
    "data": ["$A9", "$FF", "$8D", "$02", "$D0", ...]
  }
}
```

#### `debug.write_memory`

Write value to memory address.

```bash
echo '{
  "command": "debug.write_memory",
  "params": {"address": 1536, "value": 255}
}' | nc localhost 8080
```

#### `debug.add_breakpoint`

Add breakpoint at address.

```bash
echo '{
  "command": "debug.add_breakpoint",
  "params": {"address": 1536}
}' | nc localhost 8080
```

#### `debug.remove_breakpoint`

Remove breakpoint from address.

```bash
echo '{
  "command": "debug.remove_breakpoint",
  "params": {"address": 1536}
}' | nc localhost 8080
```

#### `debug.clear_breakpoints`

Remove all breakpoints.

```bash
echo '{"command": "debug.clear_breakpoints"}' | nc localhost 8080
```

#### `debug.step`

Single-step execution (one frame).

```bash
echo '{"command": "debug.step"}' | nc localhost 8080
```

#### `debug.disassemble`

Disassemble memory at address.

```bash
echo '{
  "command": "debug.disassemble",
  "params": {"address": 1536, "lines": 10}
}' | nc localhost 8080
```

### Config Commands

Manage emulator configuration settings.

#### `config.get_machine_type` / `config.set_machine_type`

Get or set machine type.

```bash
# Get current machine type
echo '{"command": "config.get_machine_type"}' | nc localhost 8080

# Set to Atari 800XL
echo '{
  "command": "config.set_machine_type",
  "params": {"machine_type": "-xl"}
}' | nc localhost 8080
```

**Valid machine types:** `-400`, `-800`, `-xl`, `-xe`, `-xegs`, `-5200`

#### `config.get_video_system` / `config.set_video_system`

Get or set video system.

```bash
# Set to PAL
echo '{
  "command": "config.set_video_system",
  "params": {"video_system": "-pal"}
}' | nc localhost 8080
```

**Valid video systems:** `-ntsc`, `-pal`

#### `config.get_basic_enabled` / `config.set_basic_enabled`

Control BASIC ROM.

```bash
# Enable BASIC
echo '{
  "command": "config.set_basic_enabled",
  "params": {"enabled": true}
}' | nc localhost 8080
```

#### `config.get_joystick_config` / `config.set_joystick_config`

Configure keyboard joystick emulation.

```bash
echo '{
  "command": "config.set_joystick_config",
  "params": {
    "kbd_joy0_enabled": true,
    "kbd_joy1_enabled": false,
    "joysticks_swapped": false
  }
}' | nc localhost 8080
```

#### `config.set_volume`

Set audio volume (0.0 to 1.0).

```bash
echo '{
  "command": "config.set_volume",
  "params": {"volume": 0.7}
}' | nc localhost 8080
```

#### `config.apply_restart`

Apply configuration changes that require restart.

```bash
echo '{"command": "config.apply_restart"}' | nc localhost 8080
```

### Status Commands

Get emulator state and server information.

#### `status.get_state`

Get current emulator state.

```bash
echo '{"command": "status.get_state"}' | nc localhost 8080
```

**Response:**
```json
{
  "result": {
    "running": true,
    "connected_clients": 2,
    "server_port": 8080,
    "pc": "$E477",
    "a": "$00"
  }
}
```

## Event System

The server broadcasts events to all connected clients when state changes occur.

### Common Events

- `connected` - Client connected with capabilities
- `disk_inserted` / `disk_ejected` - Media changes
- `system_restarted` - System reboots
- `emulation_paused` / `emulation_resumed` - Execution state
- `breakpoint_added` / `breakpoint_removed` - Debug events
- `machine_type_changed` - Configuration changes

### Example Event

```json
{
  "type": "event",
  "event": "disk_inserted",
  "data": {
    "drive": 1,
    "path": "/path/to/disk.atr"
  }
}
```

## Error Handling

### Common Error Responses

```json
{
  "type": "response",
  "status": "error",
  "error": "File not found or invalid path: /nonexistent/file.atr"
}
```

### Error Categories

- **Invalid JSON** - Malformed request
- **Missing Parameters** - Required parameter not provided
- **File Not Found** - Invalid file paths
- **Invalid Range** - Parameter out of valid range
- **State Error** - Operation not valid in current state

## Integration Examples

### IDE Integration Script

```bash
#!/bin/bash
# Example: Load and run a program

PROGRAM_PATH="/path/to/program.xex"
SERVER="localhost:8080"

# Load program
echo "{\"command\": \"media.load_xex\", \"params\": {\"path\": \"$PROGRAM_PATH\"}}" | nc $SERVER

# Wait a moment
sleep 1

# Start execution
echo '{"command": "system.resume"}' | nc $SERVER
```

### Automated Testing

```bash
#!/bin/bash
# Automated test runner

# Set up test environment
echo '{"command": "config.set_basic_enabled", "params": {"enabled": false}}' | nc localhost 8080
echo '{"command": "config.apply_restart"}' | nc localhost 8080

# Load test program
echo '{"command": "media.load_xex", "params": {"path": "/tests/test.xex"}}' | nc localhost 8080

# Set breakpoint at test completion address
echo '{"command": "debug.add_breakpoint", "params": {"address": 1536}}' | nc localhost 8080

# Run test
echo '{"command": "system.resume"}' | nc localhost 8080

# Monitor for breakpoint hit...
```

### Development Workflow

```bash
# Start emulator in development mode
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}}' | nc localhost 8080
echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/dev/disk.atr"}}' | nc localhost 8080

# Send development commands
echo '{"command": "input.send_text", "params": {"text": "LOAD \"D:PROGRAM.BAS\"\n"}}' | nc localhost 8080
echo '{"command": "input.send_text", "params": {"text": "RUN\n"}}' | nc localhost 8080
```

---

## Common Workflows

### Complete Drive Control Workflow

```bash
# Enable drive, insert disk, eject disk, disable drive
echo '{"command": "media.enable_drive", "params": {"drive": 1}}' | nc localhost 8080
echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/path/to/disk.atr"}}' | nc localhost 8080
echo '{"command": "media.eject_disk", "params": {"drive": 1}}' | nc localhost 8080  
echo '{"command": "media.disable_drive", "params": {"drive": 1}}' | nc localhost 8080
```

### Cartridge Swapping Workflow

```bash
# Load cartridge, test, eject, load different cartridge
echo '{"command": "media.insert_cartridge", "params": {"path": "/path/to/action.car"}}' | nc localhost 8080
# Test cartridge...
echo '{"command": "media.eject_cartridge"}' | nc localhost 8080
echo '{"command": "media.insert_cartridge", "params": {"path": "/path/to/basic.car"}}' | nc localhost 8080
```

### XEX Program Testing Workflow

```bash
# Load and test multiple programs with cleanup
echo '{"command": "media.load_xex", "params": {"path": "/path/to/game1.xex"}}' | nc localhost 8080
# Program runs automatically...
echo '{"command": "system.cold_boot"}' | nc localhost 8080
echo '{"command": "media.load_xex", "params": {"path": "/path/to/game2.xex"}}' | nc localhost 8080
# Program runs automatically...
echo '{"command": "system.cold_boot"}' | nc localhost 8080
```

### Visual Feedback Features

All media commands provide visual feedback in the GUI:
- **Drive Commands**: Enable/disable updates drive LEDs (off ↔ empty ↔ closed)
- **Disk Commands**: Insert/eject updates drive widgets and shows disk activity
- **Cartridge Commands**: Insert/eject updates cartridge widget (off ↔ on)
- **Status Messages**: All operations show status bar messages

---

## Notes

- All file paths must be absolute and the files must exist
- The debugger window must be open for debug commands to work
- Configuration changes may require restart - check response for `restart_required: true`
- Events are broadcast to all connected clients immediately
- Server runs on localhost only for security

For more information, see the source code in `src/tcpserver.cpp` and `include/tcpserver.h`.