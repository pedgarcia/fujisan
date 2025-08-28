# Debugger Single-Instruction Stepping Fix

## Problem Description

The Fujisan debugger had a critical issue where the "Step Into" and "Step Over" buttons were executing entire frames instead of single instructions. This meant:

- Clicking "Step" would execute 7,000-10,000 instructions instead of 1
- The PC would jump to seemingly random locations
- Precise debugging was impossible
- Breakpoints wouldn't work correctly during stepping

## Root Cause

The issue was that libatari800 only exposed frame-level execution (`libatari800_next_frame()`), not instruction-level control. The atari800 emulator core actually has full single-instruction stepping capability through its monitor, but this wasn't accessible through the library interface.

## Solution

We implemented true single-instruction stepping by:

1. **Creating a patch for libatari800** (`0003-single-instruction-stepping.patch`) that:
   - Exposes the `MONITOR_break_step` variable from the atari800 monitor
   - Adds `libatari800_step_instruction()` function that executes exactly one CPU instruction
   - Tracks instruction cycles for proper ANTIC timing

2. **Updating Fujisan's emulator interface** to add `stepOneInstruction()` method

3. **Modifying the debugger widget** to use instruction-level stepping instead of frame-level

4. **Adding TCP server support** with new `debug.step_instruction` command

## Technical Details

### How atari800's Monitor Implements Single-Stepping

The atari800 monitor uses a clever mechanism:
1. The 'G' command sets `MONITOR_break_step = TRUE`
2. `CPU_GO()` executes instructions in a loop
3. After each instruction, it checks `MONITOR_break_step`
4. If true, it calls `DO_BREAK` which returns control to the caller

### The Patch

The patch adds to libatari800:
```c
void libatari800_step_instruction(void)
{
    // Use the monitor's single-step mechanism
    MONITOR_break_step = TRUE;
    
    // Execute CPU with a small limit (one instruction will execute)
    CPU_GO(20);
    
    // Reset the step flag
    MONITOR_break_step = FALSE;
}
```

## Benefits

- **True single-instruction stepping**: Step exactly one 6502 instruction
- **Accurate debugging**: See the actual flow of execution
- **Proper Step Over**: Steps over subroutines correctly
- **Breakpoint compatibility**: Breakpoints work with stepping
- **TCP debugging support**: Remote debugging via VSCode extensions

## Building with the Patch

1. Apply the patch:
```bash
cd /path/to/atari800-src
ATARI800_SRC_PATH=$PWD ./fujisan-patches/apply-patches.sh
```

2. Build libatari800:
```bash
./autogen.sh
./configure --target=libatari800
make
```

3. Build Fujisan:
```bash
cd /path/to/fujisan
mkdir build && cd build
cmake ..
make
```

## TCP Server API

New command for single-instruction stepping:

```bash
# Step exactly one CPU instruction
echo '{"command": "debug.step_instruction"}' | nc localhost 6502
```

Response:
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

The original `debug.step` command remains for compatibility but executes a full frame.

## Testing

To verify the fix works:
1. Load a simple program with a tight loop
2. Set a breakpoint in the loop
3. Click "Step Into" - should advance exactly one instruction
4. Click "Step Over" - should step over JSR calls properly
5. PC should increment by the exact instruction size (1-3 bytes)

## Files Modified

- `/fujisan-patches/0003-single-instruction-stepping.patch` - The libatari800 patch
- `/include/atariemulator.h` - Added stepOneInstruction() declaration
- `/src/atariemulator.cpp` - Implemented stepOneInstruction()
- `/src/debuggerwidget.cpp` - Updated to use instruction-level stepping
- `/src/tcpserver.cpp` - Added debug.step_instruction command

## Future Improvements

- Add cycle-accurate stepping (step N cycles)
- Implement "Run to cursor" functionality
- Add instruction history tracking
- Support conditional breakpoints