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
3. Server starts on `localhost:6502` by default
4. Server status appears in the status bar

### Quick Test

```bash
# Test connection - should return welcome message
nc localhost 6502

# Test with curl (using HTTP-like syntax but it's raw TCP)
echo '{"command": "status.get_state"}' | nc localhost 6502
```

## Connection & Protocol

### Protocol Details

- **Transport**: Raw TCP on localhost:6502
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
curl -X POST localhost:6502 --data-raw '{
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
echo '{"command": "media.eject_disk", "params": {"drive": 1}}' | nc localhost 6502
```

#### `media.enable_drive`

Enable a disk drive (turn on).

```bash
echo '{"command": "media.enable_drive", "params": {"drive": 1}}' | nc localhost 6502
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
echo '{"command": "media.disable_drive", "params": {"drive": 1}}' | nc localhost 6502
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
}' | nc localhost 6502
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
echo '{"command": "media.eject_cartridge"}' | nc localhost 6502
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
}' | nc localhost 6502
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
echo '{"command": "system.cold_boot"}' | nc localhost 6502
```

#### `system.warm_boot`

Perform warm boot (soft restart).

```bash
echo '{"command": "system.warm_boot"}' | nc localhost 6502
```

#### `system.pause` / `system.resume`

Pause or resume emulation.

```bash
# Pause
echo '{"command": "system.pause"}' | nc localhost 6502

# Resume
echo '{"command": "system.resume"}' | nc localhost 6502
```

#### `system.set_speed`

Set emulation speed using multiplier notation (0.5x - 10x) or host speed.

```bash
# Set to 2x speed
echo '{
  "command": "system.set_speed",
  "params": {"speed": "2x"}
}' | nc localhost 6502

# Set to 0.5x speed (half speed)
echo '{
  "command": "system.set_speed",
  "params": {"speed": "0.5x"}
}' | nc localhost 6502

# Set to host speed (unlimited/as fast as possible)
echo '{
  "command": "system.set_speed",
  "params": {"speed": "host"}
}' | nc localhost 6502
```

**Parameters:**
- `speed` - Speed multiplier: `"0.5x"`, `"1x"`, `"2x"`, ... `"10x"`, or `"host"` for unlimited speed
- `percentage` - (Deprecated) Percentage value (10-1000, or 0 for unlimited). Use `speed` parameter instead.

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "speed": "2x",
    "percentage": 200
  }
}
```

**Event broadcast to all clients:**
```json
{
  "type": "event",
  "event": "speed_changed",
  "data": {
    "speed": "2x",
    "percentage": 200
  }
}
```

#### `system.get_speed`

Get current emulation speed.

```bash
echo '{"command": "system.get_speed"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "speed": "1x",
    "percentage": 100
  }
}
```

Returns both the speed in multiplier format (`"0.5x"`, `"1x"`, `"2x"`, etc., or `"host"`) and as a percentage value for backward compatibility.

#### `system.quick_save_state`

Quick save the current emulator state.

```bash
echo '{"command": "system.quick_save_state"}' | nc localhost 6502
```

Returns the quick save file path and broadcasts a `state_saved` event.

#### `system.quick_load_state`

Quick load the previously saved state.

```bash
echo '{"command": "system.quick_load_state"}' | nc localhost 6502
```

Returns the loaded profile name and broadcasts a `state_loaded` event.

#### `system.save_state`

Save the current emulator state to a specified file.

```bash
echo '{
  "command": "system.save_state",
  "params": {"filename": "/path/to/state.a8s"}
}' | nc localhost 6502
```

The `.a8s` extension is added automatically if not present.

#### `system.load_state`

Load emulator state from a specified file.

```bash
echo '{
  "command": "system.load_state",
  "params": {"filename": "/path/to/state.a8s"}
}' | nc localhost 6502
```

Returns the profile name that was saved with the state.

### Input Commands

Send keyboard input and control keys to the emulator.

#### `input.send_text`

Send a text string to the emulator.

```bash
echo '{
  "command": "input.send_text",
  "params": {"text": "LOAD \"D:HELLO.BAS\"\nRUN\n"}
}' | nc localhost 6502
```

#### `input.send_key`

Send individual keys with optional modifiers (CTRL, SHIFT, CTRL+SHIFT).

```bash
# Send Enter key
echo '{
  "command": "input.send_key",
  "params": {"key": "RETURN"}
}' | nc localhost 6502

# Send Escape
echo '{
  "command": "input.send_key",
  "params": {"key": "ESC"}
}' | nc localhost 6502

# Send CTRL+C (break)
echo '{
  "command": "input.send_key",
  "params": {"key": "c", "modifiers": ["CTRL"]}
}' | nc localhost 6502

# Send CTRL+S (save)
echo '{
  "command": "input.send_key",
  "params": {"key": "s", "modifiers": ["CTRL"]}
}' | nc localhost 6502

# Send SHIFT+A (uppercase A)
echo '{
  "command": "input.send_key",
  "params": {"key": "a", "modifiers": ["SHIFT"]}
}' | nc localhost 6502

# Send CTRL+SHIFT+A
echo '{
  "command": "input.send_key",
  "params": {"key": "a", "modifiers": ["CTRL", "SHIFT"]}
}' | nc localhost 6502

# Send arrow keys
echo '{
  "command": "input.send_key",
  "params": {"key": "UP"}
}' | nc localhost 6502
```

**Supported keys:** 
- **Letters**: `a`-`z` (case-insensitive)
- **Numbers**: `0`-`9`  
- **Special keys**: `RETURN`, `ENTER`, `SPACE`, `TAB`, `ESC`, `ESCAPE`, `BACKSPACE`
- **Arrow keys**: `UP`, `DOWN`, `LEFT`, `RIGHT`
- **Function keys**: `F1`, `F2`, `F3`, `F4`
- **Other keys**: `DELETE`, `INSERT`, `HELP`, `CLEAR`
- **Punctuation**: All standard ASCII punctuation characters

**Supported modifiers:**
- `CTRL` - Control key modifier
- `SHIFT` - Shift key modifier  
- Both can be combined: `["CTRL", "SHIFT"]`

#### `input.console_key`

Send Atari console keys.

```bash
# Press START
echo '{
  "command": "input.console_key",
  "params": {"key": "START"}
}' | nc localhost 6502

# Press SELECT
echo '{
  "command": "input.console_key",
  "params": {"key": "SELECT"}
}' | nc localhost 6502
```

**Supported console keys:** `START`, `SELECT`, `OPTION`, `RESET`

#### `input.function_key`

Send function keys F1-F4.

```bash
echo '{
  "command": "input.function_key",
  "params": {"key": "F1"}
}' | nc localhost 6502
```

#### `input.break`

Send break signal (Ctrl+C equivalent).

```bash
echo '{"command": "input.break"}' | nc localhost 6502
```

#### `input.caps_lock`

Control caps lock state with toggle, on, or off actions.

```bash
# Toggle caps lock state
echo '{
  "command": "input.caps_lock",
  "params": {"action": "toggle"}
}' | nc localhost 6502

# Turn caps lock on
echo '{
  "command": "input.caps_lock", 
  "params": {"action": "on"}
}' | nc localhost 6502

# Turn caps lock off
echo '{
  "command": "input.caps_lock",
  "params": {"action": "off"}
}' | nc localhost 6502
```

**Supported actions:**
- `toggle` - Toggle current caps lock state
- `on` - Turn caps lock on (enable)
- `off` - Turn caps lock off (disable)

#### `input.joystick`

Control joystick input for either player.

```bash
# Set player 1 joystick to UP with fire button
echo '{
  "command": "input.joystick",
  "params": {
    "player": 1,
    "direction": "UP",
    "fire": true
  }
}' | nc localhost 6502

# Set player 2 to diagonal movement
echo '{
  "command": "input.joystick",
  "params": {
    "player": 2,
    "direction": "UP_LEFT",
    "fire": false
  }
}' | nc localhost 6502

# Use numeric direction value (0-15)
echo '{
  "command": "input.joystick",
  "params": {
    "player": 1,
    "value": 14,
    "fire": true
  }
}' | nc localhost 6502
```

**Parameters:**
- `player` - Player number (1 or 2)
- `direction` - Direction string: CENTER, UP, DOWN, LEFT, RIGHT, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT
- `value` - Alternative: numeric direction value (0-15)
- `fire` - Fire button state (true/false)

#### `input.joystick_release`

Release joystick to center position.

```bash
echo '{
  "command": "input.joystick_release",
  "params": {"player": 1}
}' | nc localhost 6502
```

#### `input.get_joystick`

Get current state of specific joystick.

```bash
echo '{
  "command": "input.get_joystick",
  "params": {"player": 1}
}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "player": 1,
    "direction": "UP",
    "direction_value": 241,
    "fire": true,
    "keyboard_enabled": true,
    "keyboard_keys": "numpad"
  }
}
```

#### `input.get_all_joysticks`

Get state of all joysticks.

```bash
echo '{"command": "input.get_all_joysticks"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "joystick1": {
      "direction": "CENTER",
      "direction_value": 240,
      "fire": false,
      "keyboard_enabled": true,
      "keyboard_keys": "numpad"
    },
    "joystick2": {
      "direction": "DOWN",
      "direction_value": 242,
      "fire": true,
      "keyboard_enabled": false,
      "keyboard_keys": "wasd"
    },
    "swapped": false
  }
}
```

#### `input.start_joystick_stream`

Subscribe to real-time joystick state changes.

```bash
echo '{
  "command": "input.start_joystick_stream",
  "params": {"rate": 60}
}' | nc localhost 6502
```

**Parameters:**
- `rate` - Update rate in Hz (10-120, default 60)

**Events sent on changes:**
```json
{
  "type": "event",
  "event": "joystick_changed",
  "data": {
    "player": 1,
    "direction": "UP",
    "direction_value": 241,
    "fire": true,
    "previous_direction": "CENTER",
    "previous_fire": false,
    "timestamp": 1234567890
  }
}
```

#### `input.stop_joystick_stream`

Unsubscribe from joystick state changes.

```bash
echo '{"command": "input.stop_joystick_stream"}' | nc localhost 6502
```

#### `input.get_joystick_stream_status`

Check if client is subscribed to joystick streaming.

```bash
echo '{"command": "input.get_joystick_stream_status"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "streaming": true,
    "total_streaming_clients": 2,
    "stream_rate": 60
  }
}
```

### Debug Commands

Control debugging features, breakpoints, and memory access.

**Note:** Fujisan implements instruction-level breakpoint precision. When breakpoints are active, the emulator executes in 500-cycle chunks (approximately 280 microseconds at 1.79MHz) instead of full frames, providing 60x better breakpoint precision than frame-level debugging.

#### `debug.get_registers`

Get current CPU register values.

```bash
echo '{"command": "debug.get_registers"}' | nc localhost 6502
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
}' | nc localhost 6502
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
}' | nc localhost 6502
```

#### `debug.add_breakpoint`

Add breakpoint at address.

```bash
echo '{
  "command": "debug.add_breakpoint",
  "params": {"address": 1536}
}' | nc localhost 6502
```

#### `debug.remove_breakpoint`

Remove breakpoint from address.

```bash
echo '{
  "command": "debug.remove_breakpoint",
  "params": {"address": 1536}
}' | nc localhost 6502
```

#### `debug.clear_breakpoints`

Remove all breakpoints.

```bash
echo '{"command": "debug.clear_breakpoints"}' | nc localhost 6502
```

#### `debug.list_breakpoints` / `debug.get_breakpoints`

Get list of all current breakpoints.

```bash
echo '{"command": "debug.list_breakpoints"}' | nc localhost 6502
# or
echo '{"command": "debug.get_breakpoints"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "breakpoints": ["$2000", "$200A", "$200F"],
    "count": 3
  }
}
```

#### `debug.pause`

Pause emulation for debugging. Returns current PC value.

```bash
echo '{"command": "debug.pause"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "paused": true,
    "pc": "$2000"
  }
}
```

#### `debug.resume`

Resume emulation after pause or breakpoint hit.

```bash
echo '{"command": "debug.resume"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "resumed": true
  }
}
```

#### `debug.step`

Single-step execution (one frame). Executes approximately 7,000-10,000 instructions.

```bash
echo '{"command": "debug.step"}' | nc localhost 6502
```

#### `debug.step_instruction` / `debug.step_into`

Single-instruction stepping for precise debugging. Executes exactly one 6502 CPU instruction. Both commands are aliases and perform the same operation.

```bash
echo '{"command": "debug.step_instruction"}' | nc localhost 6502
# or
echo '{"command": "debug.step_into"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "stepped": true,
    "pc": "$2001",
    "instruction_level": true
  }
}
```

**Notes:**
- Requires emulation to be paused first
- Advances PC by the exact instruction size (1-3 bytes)
- Much more precise than `debug.step` for debugging
- Will step INTO subroutines when encountering JSR instructions
- Ideal for IDE integration and breakpoint debugging

#### `debug.step_over`

Step over subroutine calls. When encountering a JSR (Jump to Subroutine) instruction, executes the entire subroutine and returns to the instruction following the JSR. For non-JSR instructions, behaves like `step_into`.

```bash
echo '{"command": "debug.step_over"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "stepped": true,
    "pc": "$2004",
    "step_over": true
  }
}
```

**Notes:**
- Requires emulation to be paused first
- When at JSR ($20): executes entire subroutine using instruction-level stepping
- When not at JSR: executes single instruction like `step_into`
- Uses instruction-level stepping internally (not frame stepping)
- Safety limit of 10,000 instructions prevents infinite loops
- Ideal for debugging without diving into subroutine implementation

#### `debug.load_xex_for_debug`

Load an XEX file and pause after loading completes, ready for debugging. The emulator steps through frames to ensure the XEX is fully loaded into memory before pausing. This allows setting breakpoints before program execution begins.

```bash
echo '{
  "command": "debug.load_xex_for_debug",
  "params": {"path": "/path/to/program.xex"}
}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "path": "/path/to/program.xex",
    "loaded": true,
    "entry_point": "$2000",
    "paused_after_load": true
  }
}
```

**Event Sent:**
```json
{
  "type": "event",
  "event": "xex_loaded_for_debug",
  "data": {
    "path": "/path/to/program.xex",
    "entry_point": "$2000"
  }
}
```

**Notes:** 
- The emulator runs for up to 120 frames (2 seconds) to complete the loading process
- Entry point is detected from RUNAD vector ($2E0-$2E1) or INITAD vector ($2E2-$2E3)
- After loading, the emulator is paused with the program in memory
- You can now set breakpoints and use `debug.resume` to start execution

#### `debug.disassemble`

Disassemble 6502 instructions at specified address.

```bash
echo '{
  "command": "debug.disassemble",
  "params": {"address": 1536, "lines": 10}
}' | nc localhost 6502
```

Returns an array of disassembled instructions with:
- `address`: Memory address of the instruction
- `hex`: Hex bytes of the instruction (1-3 bytes)
- `instruction`: Mnemonic and operand (e.g., "LDA #$FF", "JMP $1234")

Example response:
```json
{
  "success": true,
  "result": {
    "address": "$0600",
    "lines": 10,
    "disassembly": [
      {"address": "$0600", "hex": "A9 00", "instruction": "LDA #$00"},
      {"address": "$0602", "hex": "8D 00 D4", "instruction": "STA $D400"},
      {"address": "$0605", "hex": "60", "instruction": "RTS"}
    ]
  }
}
```

#### `debug.run_loaded_xex`

Start execution of a previously loaded XEX file. Use this after `media.load_xex_no_run` to begin program execution.

```bash
echo '{"command": "debug.run_loaded_xex"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "running": true,
    "pc": "$2000"
  }
}
```

**Event sent to all clients:**
```json
{
  "type": "event",
  "event": "xex_started",
  "data": {
    "running": true,
    "pc": "$2000"
  }
}
```

**Note:** Returns an error if no XEX has been loaded with `media.load_xex_no_run`.

#### `debug.clear_loaded_xex`

Clear any XEX file that was loaded but not yet run.

```bash
echo '{"command": "debug.clear_loaded_xex"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "cleared": true
  }
}
```

#### `debug.get_xex_status`

Check if there is a loaded XEX file waiting to be run.

```bash
echo '{"command": "debug.get_xex_status"}' | nc localhost 6502
```

**Response:**
```json
{
  "type": "response",
  "status": "success",
  "result": {
    "has_loaded_xex": true
  }
}
```

### Config Commands

Manage emulator configuration settings.

#### `config.get_machine_type` / `config.set_machine_type`

Get or set machine type. Setting the machine type automatically restarts the emulator.

```bash
# Get current machine type
echo '{"command": "config.get_machine_type"}' | nc localhost 6502

# Set to Atari 800XL
echo '{
  "command": "config.set_machine_type",
  "params": {"machine_type": "xl"}
}' | nc localhost 6502
```

**Valid machine types:** `400`, `800`, `xl`, `xe`, `xegs`, `5200`

#### `config.get_video_system` / `config.set_video_system`

Get or set video system. Setting the video system automatically restarts the emulator.

```bash
# Get current video system
echo '{"command": "config.get_video_system"}' | nc localhost 6502

# Set to PAL
echo '{
  "command": "config.set_video_system",
  "params": {"video_system": "pal"}
}' | nc localhost 6502

# Set to NTSC
echo '{
  "command": "config.set_video_system",
  "params": {"video_system": "ntsc"}
}' | nc localhost 6502
```

**Valid video systems:** `ntsc`, `pal`

#### `config.get_basic_enabled` / `config.set_basic_enabled`

Control BASIC ROM.

```bash
# Enable BASIC
echo '{
  "command": "config.set_basic_enabled",
  "params": {"enabled": true}
}' | nc localhost 6502
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
}' | nc localhost 6502
```

#### `config.set_volume`

Set audio volume (0.0 to 1.0).

```bash
echo '{
  "command": "config.set_volume",
  "params": {"volume": 0.7}
}' | nc localhost 6502
```

#### `config.apply_restart`

Apply configuration changes that require restart.

```bash
echo '{"command": "config.apply_restart"}' | nc localhost 6502
```

#### `config.get_profiles`

Get list of available configuration profiles and current profile.

```bash
echo '{"command": "config.get_profiles"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "profiles": ["Default", "Development", "Gaming", "FujiNet"],
    "current": "Default"
  },
  "status": "ok"
}
```

#### `config.get_current_profile`

Get the currently active profile name.

```bash
echo '{"command": "config.get_current_profile"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "profile": "Default"
  },
  "status": "ok"
}
```

#### `config.load_profile`

Load a configuration profile by name.

```bash
echo '{
  "command": "config.load_profile",
  "params": {"profile_name": "Development"}
}' | nc localhost 6502
```

**Note:** Loading a profile applies all its settings immediately and may trigger an emulator restart if required.

### Screen Commands

Capture and access screen data.

#### `screen.capture`

Save a screenshot of the current screen to a file.

```bash
# Save screenshot with default filename
echo '{"command": "screen.capture"}' | nc localhost 6502

# Save with specific filename (automatically converts to PCX format)
echo '{
  "command": "screen.capture",
  "params": {"filename": "my_screenshot.png"}
}' | nc localhost 6502

# Save interlaced screenshot
echo '{
  "command": "screen.capture",
  "params": {
    "filename": "interlaced.pcx",
    "interlaced": true
  }
}' | nc localhost 6502
```

**Note**: Screenshots are saved in PCX format. If you specify a different extension (e.g., .png), it will be automatically changed to .pcx.

**Response includes:**
- `filename` - Full path to saved screenshot
- `interlaced` - Whether interlaced mode was used
- `timestamp` - When screenshot was taken
- `size_bytes` - File size in bytes

### Status Commands

Get emulator state and server information.

#### `status.get_state`

Get current emulator state.

```bash
echo '{"command": "status.get_state"}' | nc localhost 6502
```

**Response:**
```json
{
  "result": {
    "running": true,
    "connected_clients": 2,
    "server_port": 6502,
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
SERVER="localhost:6502"

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
echo '{"command": "config.set_basic_enabled", "params": {"enabled": false}}' | nc localhost 6502
echo '{"command": "config.apply_restart"}' | nc localhost 6502

# Load test program
echo '{"command": "media.load_xex", "params": {"path": "/tests/test.xex"}}' | nc localhost 6502

# Set breakpoint at test completion address
echo '{"command": "debug.add_breakpoint", "params": {"address": 1536}}' | nc localhost 6502

# Run test
echo '{"command": "system.resume"}' | nc localhost 6502

# Monitor for breakpoint hit...
```

### Debugging with Breakpoints

```bash
#!/bin/bash
# Debug workflow - load program and set breakpoints before execution

# Load XEX file for debugging (loads completely then pauses)
echo '{"command": "debug.load_xex_for_debug", "params": {"path": "/path/to/debug.xex"}}' | nc localhost 6502

# Wait a moment for loading to complete
sleep 1

# Set breakpoints at critical addresses while paused
echo '{"command": "debug.add_breakpoint", "params": {"address": 8192}}' | nc localhost 6502
echo '{"command": "debug.add_breakpoint", "params": {"address": 8256}}' | nc localhost 6502

# Resume execution - will stop at breakpoints
echo '{"command": "debug.resume"}' | nc localhost 6502
```

### Development Workflow

```bash
# Start emulator in development mode
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}}' | nc localhost 6502
echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/dev/disk.atr"}}' | nc localhost 6502

# Send development commands
echo '{"command": "input.send_text", "params": {"text": "LOAD \"D:PROGRAM.BAS\"\n"}}' | nc localhost 6502
echo '{"command": "input.send_text", "params": {"text": "RUN\n"}}' | nc localhost 6502
```

---

## Common Workflows

### Complete Drive Control Workflow

```bash
# Enable drive, insert disk, eject disk, disable drive
echo '{"command": "media.enable_drive", "params": {"drive": 1}}' | nc localhost 6502
echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/path/to/disk.atr"}}' | nc localhost 6502
echo '{"command": "media.eject_disk", "params": {"drive": 1}}' | nc localhost 6502  
echo '{"command": "media.disable_drive", "params": {"drive": 1}}' | nc localhost 6502
```

### Cartridge Swapping Workflow

```bash
# Load cartridge, test, eject, load different cartridge
echo '{"command": "media.insert_cartridge", "params": {"path": "/path/to/action.car"}}' | nc localhost 6502
# Test cartridge...
echo '{"command": "media.eject_cartridge"}' | nc localhost 6502
echo '{"command": "media.insert_cartridge", "params": {"path": "/path/to/basic.car"}}' | nc localhost 6502
```

### XEX Program Testing Workflow

```bash
# Load and test multiple programs with cleanup
echo '{"command": "media.load_xex", "params": {"path": "/path/to/game1.xex"}}' | nc localhost 6502
# Program runs automatically...
echo '{"command": "system.cold_boot"}' | nc localhost 6502
echo '{"command": "media.load_xex", "params": {"path": "/path/to/game2.xex"}}' | nc localhost 6502
# Program runs automatically...
echo '{"command": "system.cold_boot"}' | nc localhost 6502
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