⏺ TCP Server Manual Test Plan

  Connection & Basic Protocol

  - Connect to server on localhost:6502 using nc
  - Verify welcome message with capabilities
  - Test multiple simultaneous client connections
  - Test client disconnect/reconnect
  - Verify server survives client crash/abrupt disconnect

  JSON Format Compatibility

  - Test single-line compact JSON
  - Test multi-line formatted JSON
  - Test JSON with extra whitespace
  - Test invalid JSON handling
  - Test incomplete JSON (buffer handling)
  - Test very large JSON payload (>1MB limit)

  Media Commands - Disk Operations

  - media.insert_disk - Insert disk into D1-D8
    ```bash
    # Insert disk into D1
    echo '{"command": "media.insert_disk", "params": {"drive": 1, "path": "/Users/pgarcia/dev/atari/roms/HardwareTester.atr"}}' | nc localhost 6502

    # Insert disk into D2
    echo '{"command": "media.insert_disk", "params": {"drive": 2, "path": "/Users/pgarcia/dev/atari/roms/Mega Maze.atr"}}' | nc localhost 6502

    # Test all drives (D1-D8)
    for drive in {1..8}; do
      echo "{\"command\": \"media.insert_disk\", \"params\": {\"drive\": $drive, \"path\": \"/Users/pgarcia/dev/atari/roms/HardwareTester.atr\"}}" | nc localhost 6502
      sleep 1
    done
    ```

  - media.eject_disk - Eject disk from D1-D8
    ```bash
    # Eject disk from D1
    echo '{"command": "media.eject_disk", "params": {"drive": 1}}' | nc localhost 6502

    # Eject disk from D2
    echo '{"command": "media.eject_disk", "params": {"drive": 2}}' | nc localhost 6502

    # Eject all drives (D1-D8)
    for drive in {1..8}; do
      echo "{\"command\": \"media.eject_disk\", \"params\": {\"drive\": $drive}}" | nc localhost 6502
      sleep 1
    done
    ```

  - media.enable_drive - Enable drive D1-D8
    ```bash
    # Enable D1
    echo '{"command": "media.enable_drive", "params": {"drive": 1}}' | nc localhost 6502

    # Enable D2
    echo '{"command": "media.enable_drive", "params": {"drive": 2}}' | nc localhost 6502

    # Enable all drives (D1-D8)
    for drive in {1..8}; do
      echo "{\"command\": \"media.enable_drive\", \"params\": {\"drive\": $drive}}" | nc localhost 6502
      sleep 1
    done
    ```

  - media.disable_drive - Disable drive D1-D8
    ```bash
    # Disable D1
    echo '{"command": "media.disable_drive", "params": {"drive": 1}}' | nc localhost 6502

    # Disable D2
    echo '{"command": "media.disable_drive", "params": {"drive": 2}}' | nc localhost 6502

    # Disable all drives (D1-D8)
    for drive in {1..8}; do
      echo "{\"command\": \"media.disable_drive\", \"params\": {\"drive\": $drive}}" | nc localhost 6502
      sleep 1
    done
    ```

  - Verify UI updates when disk inserted via TCP
  - Verify UI updates when disk ejected via TCP
  - Test invalid drive numbers (0, 9, negative)
    ```bash
    # Test invalid drive 0
    echo '{"command": "media.insert_disk", "params": {"drive": 0, "path": "/Users/pgarcia/dev/atari/roms/HardwareTester.atr"}}' | nc localhost 6502

    # Test invalid drive 9
    echo '{"command": "media.insert_disk", "params": {"drive": 9, "path": "/Users/pgarcia/dev/atari/roms/HardwareTester.atr"}}' | nc localhost 6502

    # Test negative drive number
    echo '{"command": "media.insert_disk", "params": {"drive": -1, "path": "/Users/pgarcia/dev/atari/roms/HardwareTester.atr"}}' | nc localhost 6502
    ```

  Media Commands - Cartridge Operations

  - media.insert_cartridge - Insert .rom/.car/.bin file
  - media.eject_cartridge - Eject cartridge
  - Verify cartridge widget shows "loaded" state
  - Verify cartridge widget shows "empty" state after eject
  - Test with various cartridge formats (.rom, .car, .bin)

  Media Commands - XEX/Binary Loading

  - media.load_xex - Load XEX executable
  - Verify XEX loads and runs
  - Test with various XEX file sizes

  System Commands - Boot/Reset

  - system.cold_boot - Perform cold boot
  - system.warm_boot - Perform warm boot
  - system.restart - Generic restart
  - Verify emulator state resets properly

  System Commands - Pause/Resume

  - system.pause - Pause emulation
  - system.resume - Resume emulation
  - Test pause when already paused (error handling)
  - Test resume when not paused (error handling)
  - Verify UI reflects paused state

  System Commands - Speed Control

  - system.set_speed - Test speed values: "0.5x", "1x", "2x", "5x", "10x", "host"
  - Test legacy percentage parameter: 10%, 50%, 100%, 200%, 1000%
  - Test invalid speeds (e.g., "0.4x", "11x", invalid format)
  - Verify speed change event broadcast to all clients with both speed and percentage
  - system.get_speed - Get current speed
  - Verify response includes both speed ("1x", "2x", "host") and percentage (100, 200, 0)

  System Commands - Save States

  - system.quick_save_state - Save quick state
  - system.quick_load_state - Load quick state
  - system.save_state - Save to specific filename
  - system.load_state - Load from specific filename
  - Test save/load with profile names
  - Test loading non-existent state file

  Input Commands - Text & Keys

  - input.send_text - Send simple text (letters, numbers)
  - Test special characters in text
  - Test very long text strings
  - input.send_key - Test all special keys (RETURN, ESC, BACKSPACE, arrows)
  - Test uppercase/lowercase letters
  - Test key modifiers (SHIFT, CTRL)

  Input Commands - Console Keys

  - input.console_key - Test START, SELECT, OPTION, RESET
  - input.break - Send break signal
  - input.function_key - Test F1-F4
  - input.caps_lock - Toggle/on/off

  Input Commands - Joystick

  - input.joystick - Test all 8 directions + center
  - Test fire button on/off
  - Test both players (1 and 2)
  - input.joystick_release - Release to center
  - input.get_joystick - Get current joystick state
  - input.get_all_joysticks - Get all joystick states
  - input.start_joystick_stream - Subscribe to joystick changes
  - input.stop_joystick_stream - Unsubscribe
  - Test joystick streaming with multiple clients

  Config Commands - Machine Settings

  - config.get_machine_type - Get current machine
  - config.set_machine_type - Test all types (-400, -800, -xl, -xe, -xegs, -5200)
  - config.get_video_system - Get PAL/NTSC
  - config.set_video_system - Switch between PAL/NTSC
  - Test invalid machine types/video systems

  Config Commands - BASIC & ROM

  - config.get_basic_enabled - Get BASIC status
  - config.set_basic_enabled - Enable/disable BASIC
  - config.get_rom_paths - Get ROM file paths
  - config.set_rom_paths - Set OS and BASIC ROM paths
  - Test with invalid ROM paths

  Config Commands - Joystick & Audio

  - config.get_joystick_config - Get joystick settings
  - config.set_joystick_config - Configure keyboard joystick and swap
  - config.get_audio_config - Get audio enabled status
  - config.set_audio_enabled - Enable/disable audio
  - config.set_volume - Test 0.0 to 1.0 volume range
  - Test invalid volume values (<0, >1.0)

  Config Commands - SIO & Advanced

  - config.get_sio_patch - Get SIO patch status
  - config.set_sio_patch - Enable/disable fast disk I/O
  - config.apply_restart - Apply config changes requiring restart

  Config Commands - Profiles

  - config.get_profiles - List all configuration profiles
  - config.get_current_profile - Get active profile name
  - config.load_profile - Load specific profile
  - Test loading non-existent profile

  Status & Screen Commands

  - status.get_state - Get emulator state (running, paused, PC, registers)
  - screen.capture - Save screenshot to file
  - Test screenshot with/without interlace
  - Test screenshot filename generation
  - Verify screenshot file created

  Debug Commands (if debugger enabled)

  - debug.get_registers - Get CPU register values
  - debug.read_memory - Read memory at address
  - debug.write_memory - Write to memory address
  - debug.add_breakpoint - Add breakpoint
  - debug.remove_breakpoint - Remove breakpoint
  - debug.list_breakpoints - List all breakpoints
  - debug.clear_breakpoints - Clear all breakpoints
  - debug.pause / debug.resume - Debug pause/resume
  - debug.step - Step one frame
  - debug.step_instruction - Step one CPU instruction
  - debug.step_over - Step over JSR calls
  - debug.disassemble - Disassemble at address
  - debug.load_xex_for_debug - Load XEX with entry breakpoint

  Path Handling

  - Test absolute paths with leading /
  - Test absolute paths without leading / (auto-fix)
  - Test relative paths
  - Test paths with spaces
  - Test paths with special characters
  - Test non-existent file paths (error handling)
  - Test paths with escaped characters

  Error Handling

  - Test unknown command categories
  - Test unknown subcommands
  - Test missing required parameters
  - Test invalid parameter types (string vs int)
  - Test invalid parameter values (out of range)
  - Test malformed requests (missing "command" field)
  - Verify error responses have proper format

  Event Broadcasting

  - Connect multiple clients
  - Trigger events (disk insert, cartridge insert, speed change)
  - Verify all clients receive event notifications
  - Test events during client disconnect

  Performance & Limits

  - Test rapid command succession
  - Test buffer overflow protection (>1MB JSON)
  - Test server stability with continuous connections
  - Test memory leaks with repeated connect/disconnect

