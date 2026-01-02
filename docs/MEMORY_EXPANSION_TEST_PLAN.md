# Memory Expansion Test Plan

This document provides testing instructions for Fujisan's memory expansion features: Mosaic RAM, Axlon RAM, and MapRAM.

## Overview

Fujisan supports three types of memory expansion for Atari 8-bit emulation:

1. **Mosaic RAM** - 4KB bank switching memory expansion (48-300 KB, increments of 4 KB)
2. **Axlon RAM** - 16KB bank switching memory expansion (32, 160, 288, 544, 1056 KB only)
3. **MapRAM** - XL/XE extended memory banking (default: enabled)

These settings are configured in **Settings → Hardware → Memory Configuration** and are passed to libatari800 during emulator initialization.

## Automatic Settings Migration

**Important**: If you're upgrading from a previous version of Fujisan that had invalid memory configuration values, the settings will be automatically migrated on first launch:

- **Mosaic RAM > 300 KB**: Clamped to 300 KB (maximum)
- **Mosaic RAM < 48 KB**: Set to 128 KB (default)
- **Mosaic RAM not divisible by 4**: Rounded to nearest valid value
- **Axlon RAM invalid size**: Set to 160 KB (default)
- **MapRAM not configured**: Enabled by default

Migration is logged to console and automatically saves corrected values to prevent repeated warnings.

## Configuration

### Accessing Memory Settings

1. Launch Fujisan
2. Go to **Fujisan → Settings** (or press `Cmd+,` on macOS)
3. Select the **Hardware** tab
4. Scroll to the **Memory Configuration** section

### Available Options

#### Mosaic RAM Expansion
- **Enable Mosaic RAM expansion** - Checkbox to enable/disable
- **Size selector** - Spinbox for total RAM size (48-300 KB in 4 KB increments)
- **Default**: Disabled, 128 KB when enabled
- **Valid range**: 48 KB to 300 KB (must be 48 + multiple of 4)
- **Command-line parameter**: `-mosaic <size>`

#### Axlon RAM Expansion
- **Enable Axlon RAM expansion** - Checkbox to enable/disable
- **Size selector** - Dropdown with only valid sizes:
  - 32 KB (0 banks)
  - 160 KB (8 banks)
  - 288 KB (16 banks)
  - 544 KB (32 banks)
  - 1056 KB (64 banks)
- **Use Axlon shadow at 0x0FC0-0x0FFF** - Enables shadow banking mode
- **Default**: Disabled, 160 KB when enabled
- **Command-line parameters**: `-axlon <size>` and optional `-axlon0f`

#### MapRAM
- **Enable MapRAM (XL/XE machines)** - Checkbox to enable/disable
- **Default**: Enabled (checked)
- **Command-line parameters**: `-mapram` or `-no-mapram`

## Verification Methods

### Method 1: Debug Console Output (Recommended)

This is the quickest way to verify memory parameters are being passed to libatari800.

1. **Enable debug output** (if not already visible):
   - On macOS: Run Fujisan from Terminal: `/Applications/Fujisan.app/Contents/MacOS/Fujisan`
   - On Windows: Check the console window
   - On Linux: Run from terminal

2. **Configure memory settings**:
   - Open Settings → Hardware
   - Enable Mosaic RAM, set to 128 KB
   - Enable Axlon RAM, set to 288 KB (16 banks), enable shadow
   - Ensure MapRAM is enabled
   - Click **OK** (this restarts the emulator)

3. **Check console output** for the argument list:
   ```
   === COMPLETE ARGUMENT LIST FOR libatari800_init() ===
     Arguments: atari800 -xl -pal -mosaic 128 -axlon 288 -axlon0f -mapram -sound ...
   ```

4. **Verify presence of**:
   - `-mosaic 128` - Mosaic RAM enabled at 128 KB
   - `-axlon 288` - Axlon RAM enabled at 288 KB (16 banks)
   - `-axlon0f` - Axlon shadow mode enabled
   - `-mapram` - MapRAM enabled (or `-no-mapram` if disabled)

### Method 2: BASIC XE Memory Test

This tests whether extended RAM is actually working in the emulator.

**Requirements:**
- Atari 800XL or 130XE machine type
- BASIC enabled
- Mosaic or Axlon RAM enabled

**Test Procedure:**

1. **Configure the emulator**:
   - Settings → Hardware → System Configuration
   - Set Machine Type to "800XL" or "130XE"
   - Enable BASIC
   - Enable Mosaic RAM with 128 KB (or Axlon RAM with 160 KB)
   - Click OK to restart

2. **Boot to BASIC** (you should see `READY`)

3. **Run memory test**:
   ```basic
   10 DIM A$(60000)
   20 PRINT "Success! Extended RAM is working"
   30 PRINT FRE(0);" bytes free"
   ```

4. **Expected results**:
   - **With extended RAM**: Program runs, prints success message and shows large free memory
   - **Without extended RAM**: `?OUT OF MEMORY ERROR IN 10`

### Method 3: Machine Language Test

For advanced users who want to verify memory banking directly.

**Mosaic RAM Bank Test:**

```basic
10 REM Test Mosaic RAM banks
20 BANK=0:REM Bank number
30 POKE 55296,BANK:REM Select bank
40 PEEK(49152):REM Read from banked area
50 PRINT "Bank ";BANK;" accessed"
```

**Axlon RAM Bank Test:**

```basic
10 REM Test Axlon RAM banks
20 BANK=0:REM Bank number
30 POKE 53760,BANK:REM Select bank (0xD200)
40 PEEK(49152):REM Read from banked area
50 PRINT "Bank ";BANK;" accessed"
```

### Method 4: TCP Server API Inspection

If the TCP debugger server is enabled, you can inspect memory configuration.

1. **Enable TCP server**:
   - Settings → Emulator → TCP Server
   - Enable "TCP Server for Debugging"
   - Set port (default: 6502)
   - Click OK

2. **Connect via telnet**:
   ```bash
   telnet localhost 6502
   ```

3. **Inspect memory** (future enhancement):
   ```
   info.memory
   ```

   This should show configured memory expansions (requires implementation).

## Test Scenarios

### Scenario 1: Basic Configuration Test

**Objective**: Verify settings are saved and loaded correctly

1. Open Settings → Hardware
2. Enable Mosaic RAM, set to 128 KB
3. Click OK
4. Reopen Settings → Hardware
5. **Verify**: Mosaic RAM is still enabled at 128 KB

**Expected Result**: Settings persist across dialog open/close

### Scenario 2: Mosaic vs Axlon Conflict

**Objective**: Verify both can be enabled simultaneously (or test conflicts)

1. Enable both Mosaic RAM (128 KB) and Axlon RAM (288 KB)
2. Click OK
3. Check debug console

**Expected Result**:
- Both `-mosaic 128` and `-axlon 288` appear in arguments
- Note: Some software may only work with one type - this is expected

### Scenario 3: Configuration Profile Test

**Objective**: Verify memory settings are saved in profiles

1. Open Settings → Hardware
2. Enable Mosaic RAM at 128 KB
3. Save as a new profile: "Mosaic Test"
4. Disable Mosaic RAM
5. Load profile "Mosaic Test"

**Expected Result**: Mosaic RAM is re-enabled at 128 KB

### Scenario 4: MapRAM Compatibility

**Objective**: Test MapRAM with different machine types

1. **Test with XL/XE machines**:
   - Set machine type to 800XL
   - Enable MapRAM
   - Verify `-mapram` in console
   - Test with software that uses banking

2. **Test with 800/5200 machines**:
   - Set machine type to Atari 800
   - MapRAM setting should have no effect (XL/XE only)

**Expected Result**: MapRAM only affects XL/XE machines

### Scenario 5: Memory-Intensive Software

**Objective**: Test with real software that requires extended RAM

**Recommended Test Software:**
- BASIC XE programs with large arrays
- RAM-disk utilities
- Development tools (assemblers, compilers)
- Games that support extended RAM

**Procedure:**
1. Enable appropriate memory expansion
2. Load test software
3. Verify software detects and uses extended RAM

## Common Issues and Troubleshooting

### Issue 1: "Out of Memory" Error Despite Extended RAM

**Possible Causes:**
- Memory expansion not properly enabled
- Wrong memory type for the software (some need Mosaic, others Axlon)
- Machine type doesn't support expansion (e.g., 5200)

**Solution:**
- Check console output for `-mosaic` or `-axlon` parameters
- Try alternative expansion type
- Ensure using XL/XE machine type

### Issue 2: Settings Not Applied

**Possible Causes:**
- Settings dialog cancelled instead of OK clicked
- Emulator not restarted after changes

**Solution:**
- Always click OK to apply and restart
- Check console for "INITIALIZATION COMPLETE" message

### Issue 3: Parameters Not in Console Output

**Possible Causes:**
- Checkbox not enabled (only size selector changed)
- Settings not saved properly

**Solution:**
- Ensure checkbox is checked, not just size selected
- Verify QSettings storage in debug output

## Implementation Details

### Code Locations

Memory configuration is implemented in:
- **UI Controls**: `src/settingsdialog.cpp` lines 228-280
- **Settings Storage**: `src/settingsdialog.cpp` lines 2018-2025, 2443-2449
- **Argument Building**: `src/atariemulator.cpp` lines 462-491
- **Profile Storage**: `include/configurationprofile.h` lines 34-38

### Command-Line Parameters

The implementation passes these parameters to libatari800:

```cpp
// Mosaic RAM (if enabled)
argList << "-mosaic" << QString::number(mosaicSize);

// Axlon RAM (if enabled)
argList << "-axlon" << QString::number(axlonSize);
if (axlonShadow) {
    argList << "-axlon0f";
}

// MapRAM
if (enableMapRam) {
    argList << "-mapram";
} else {
    argList << "-no-mapram";
}
```

### libatari800 Memory Variables

These extern variables are set by the command-line parameters:

```c
extern int MEMORY_mosaic_num_banks;  // Set by -mosaic
extern int MEMORY_axlon_num_banks;   // Set by -axlon
extern int MEMORY_enable_mapram;     // Set by -mapram/-no-mapram
```

## Known Limitations

### Enable 800 RAM Option (Disabled)

The "Enable RAM at 0xC000-0xCFFF (Atari 800)" option has been disabled because:
- No direct command-line parameter exists in libatari800
- The Atari 800 machine type (`-atari`) defaults to 48KB which includes this RAM region
- Controlling this would require direct manipulation of `MEMORY_ram_size` after initialization

If finer control is needed in the future, the implementation would need to:
1. Call `libatari800_init()` with standard parameters
2. Directly set `extern int MEMORY_ram_size` after initialization
3. Call `Atari800_InitialiseMachine()` to apply changes

## Test Checklist

Use this checklist when testing memory expansion features:

- [ ] Mosaic RAM parameters appear in console output when enabled
- [ ] Axlon RAM parameters appear in console output when enabled
- [ ] Axlon shadow (`-axlon0f`) appears when checkbox enabled
- [ ] MapRAM parameter appears (enabled by default)
- [ ] BASIC XE memory test succeeds with extended RAM
- [ ] BASIC XE memory test fails without extended RAM (confirms toggle works)
- [ ] Settings persist after closing and reopening Settings dialog
- [ ] Configuration profiles save/load memory settings correctly
- [ ] Settings cause emulator restart (initialization message in console)
- [ ] Different RAM sizes work (Mosaic: 48, 128, 200, 300 KB; Axlon: 32, 160, 288, 544, 1056 KB)
- [ ] Disabling expansions removes parameters from console output
- [ ] Real software that requires extended RAM works correctly

## Success Criteria

Memory expansion implementation is working correctly when:

1. **Configuration UI** - All controls are visible and functional in Settings dialog
2. **Parameter Passing** - Memory parameters appear in console "COMPLETE ARGUMENT LIST"
3. **Functional Testing** - BASIC XE test can allocate large arrays with expansion enabled
4. **Persistence** - Settings survive restart and are saved in configuration profiles
5. **Integration** - Real Atari software that needs extended RAM works correctly

## Reporting Issues

When reporting memory expansion issues, please include:

1. **Settings Configuration**:
   - Machine type (800, 800XL, 130XE, etc.)
   - Which memory expansion enabled (Mosaic/Axlon/MapRAM)
   - Size settings
   - Shadow mode setting (for Axlon)

2. **Console Output**:
   - Complete "COMPLETE ARGUMENT LIST" line
   - Any error messages during initialization

3. **Test Results**:
   - What software/test was used
   - Expected vs actual behavior
   - Screenshot of error (if applicable)

4. **System Information**:
   - Fujisan version
   - Operating system (macOS/Windows/Linux)
   - Any custom configuration profiles

## References

- **libatari800 Documentation**: See atari800 source code `src/atari.c` for parameter handling
- **Memory Expansion Types**: See `src/memory.c` for implementation details
- **Configuration Profiles**: See `docs/CONFIGURATION_PROFILES.md` (if it exists)
