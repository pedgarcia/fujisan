# FujiNet Drive UI Integration Plan

> **Note:** For comprehensive FujiNet-PC information including binary management, protocol details, and source code locations, see `docs_local/FUJINET_PC.md` (private documentation).

## Overview
Integrate Fujisan's drive UI (D1-D8) with FujiNet-PC's web API to show and manage FujiNet drives when NetSIO is enabled.

## Research Findings

### FujiNet API Structure

The FujiNet-PC web API (default: http://localhost:8000) uses HTML-based responses with embedded data attributes, not JSON.

#### Available Endpoints

**Health Check** - `GET /test`
- Response: `{"result": 1}` (JSON)
- Purpose: Verify FujiNet is running

**Drive Status** - `GET /` (root)
- Response: HTML with `data-mount` attributes
- Format for each drive slot:
  ```html
  <div data-mountslot="0" data-mount="(Empty)">Drive Slot 1</div>
  <div data-mountslot="1" data-mount="/ANIMALS.XEX (R)">Drive Slot 2</div>
  ```
- Mount string format:
  - `"(Empty)"` - No disk mounted
  - `"/filename (R)"` - Read-only disk
  - `"/filename (W)"` - Read-write disk

**Unmount** - `GET /unmount?deviceslot=N`
- Parameters: `deviceslot` (0-7 for D1-D8)
- Response: HTTP 200 with HTML confirmation

**Mount** - `GET /mount?deviceslot=N&host=H&file=/path&mode=M`
- Parameters:
  - `deviceslot`: 0-7 for D1-D8
  - `host`: Host number (1=SD, 2-8=network hosts)
  - `file`: File path (e.g., `/ANIMALS.XEX`)
  - `mode`: `r` (read-only) or `w` (read-write)
- Response: HTTP 200

**Host List** - `GET /hosts`
- Response: Plain text, one host per line
- Example: `SD\nfujinet.online\nfujinet.pl`

**File Browser** - `GET /browse/host/N`
- Response: HTML table with file listing
- Can browse SD card (host 1) and network hosts (2-8)

### Existing Fujisan Code

**FujiNetService** (`include/fujinetservice.h`, `src/fujinetservice.cpp`)
- Complete REST API client using Qt's QNetworkAccessManager
- Methods: `mount()`, `unmount()`, `queryMountStatus()`, health check
- Signals for all operations: success, failure, status updates
- **Current Issue**: Methods use incorrect API parameters (needs fixing)

**DiskDriveWidget** (`include/diskdrivewidget.h`, `src/diskdrivewidget.cpp`)
- Currently manages local disk images only
- Shows drive state, activity LEDs, filename
- Supports drag-and-drop insertion
- Valid extensions: .atr, .xfd, .dcm

**Settings Dialog** (`src/settingsdialog.cpp`)
- Has FujiNetService instance (line 1842)
- FujiNet SD path setting: `m_fujinetSDPath` (already exists)
- FujiNet process management and health check

## Implementation Approach

### Phase 1: FujiNetService API Enhancements

**File**: `src/fujinetservice.cpp`, `include/fujinetservice.h`

1. **Add HTML parsing for drive status**
   - Parse `GET /` response to extract `data-mount` attributes for drives 1-8
   - Create method `queryDriveStatus()` that returns list of drive states
   - Parse mount format: `"/filename (R)"` or `"/filename (W)"` or `"(Empty)"`
   - Create `FujiNetDrive` struct:
     ```cpp
     struct FujiNetDrive {
         int slotNumber;      // 0-7 for D1-D8
         QString filename;    // Empty string if unmounted
         bool isEmpty;        // true if no disk mounted
         bool isReadOnly;     // true for (R), false for (W)
     };
     ```
   - Emit signal `driveStatusUpdated(QVector<FujiNetDrive>)`

2. **Implement file copy to SD folder**
   - Add method `copyToSD(QString localPath) -> QString`
   - Use existing `m_fujinetSDPath` setting from Settings dialog
   - Return the SD-relative path for mounting (e.g., `/mydisk.atr`)
   - Handle file naming conflicts (append numbers if needed: `disk_1.atr`, `disk_2.atr`)
   - Emit progress signals for large files: `copyProgress(int percent, QString filename)`
   - Use QFile::copy() for file operations

3. **Fix mount/unmount methods**
   - Update `mount()` to use correct parameters: `deviceslot`, `host`, `file`, `mode`
   - Default: `host=1` (SD card), `mode=w` (read-write, always)
   - Update `unmount()` to use correct parameter: `deviceslot` (0-7)
   - Build URLs properly: `GET /mount?deviceslot=0&host=1&file=/disk.atr&mode=w`

4. **Add periodic polling**
   - Implement QTimer-based polling of drive status (every 2-3 seconds)
   - Add methods: `startDrivePolling()`, `stopDrivePolling()`
   - Only poll when FujiNet is connected and NetSIO is enabled
   - Stop polling when disconnected or NetSIO disabled
   - Call `queryDriveStatus()` on each timer tick

### Phase 2: Drive UI Mode Switching

**File**: `src/diskdrivewidget.cpp`, `include/diskdrivewidget.h`

1. **Add FujiNet mode support**
   - Add enum:
     ```cpp
     enum DriveMode { LOCAL, FUJINET };
     ```
   - Add member variable: `DriveMode m_driveMode`
   - Add method: `void setDriveMode(DriveMode mode)`
   - Add method: `void updateFromFujiNet(const FujiNetDrive& driveInfo)`
   - Add visual mode indicator (subtle blue background tint for FujiNet mode)

2. **Update drive display logic**
   - When mode is LOCAL: use existing `updateFromEmulator()`
   - When mode is FUJINET: use new `updateFromFujiNet()`
   - Show filename from FujiNet data
   - Show R/W indicator (though we always mount as R/W)
   - Update LED states based on FujiNet activity

3. **Update insert/eject operations**
   - LOCAL mode: existing behavior (mount to Atari800 core)
   - FUJINET mode:
     - Show spinner overlay on drive widget during copy
     - Call `FujiNetService::copyToSD(localPath)` with progress tracking
     - Call `FujiNetService::mount(driveNumber, sdPath)` with mode=w
     - Update status bar with operation status
     - Handle errors with user dialogs
   - Add method: `void showCopyProgress(bool show)` - displays spinner overlay

4. **Enhanced tooltips for FujiNet mode**
   - Show: filename, full path, R/W mode, host, connection status
   - Example tooltip:
     ```
     D1: /disk.atr (R/W)
     Host: SD
     Status: Connected
     Click to eject
     ```
   - Empty drive tooltip:
     ```
     D1: Empty
     Drag disk image here to mount to FujiNet
     ```

### Phase 3: MainWindow Integration

**File**: `src/mainwindow.cpp`, `include/mainwindow.h`

1. **Connect FujiNetService to MainWindow**
   - Create FujiNetService member (share instance with Settings)
   - Connect to NetSIO enabled/disabled state changes
   - Start/stop FujiNet drive polling based on NetSIO state:
     ```cpp
     void onNetSIOStateChanged(bool enabled) {
         if (enabled && fujinetConnected) {
             m_fujinetService->startDrivePolling();
             switchDrivesToFujiNetMode();
         } else {
             m_fujinetService->stopDrivePolling();
             switchDrivesToLocalMode();
         }
     }
     ```

2. **Update all drive widgets**
   - D1 in toolbar, D3-D8 in drawer
   - When NetSIO enabled: call `setDriveMode(FUJINET)` on all drives
   - When NetSIO disabled: call `setDriveMode(LOCAL)` on all drives
   - Connect `driveStatusUpdated()` signal to update all drive UIs:
     ```cpp
     connect(m_fujinetService, &FujiNetService::driveStatusUpdated,
             this, &MainWindow::updateDriveWidgetsFromFujiNet);
     ```

3. **Handle FujiNet connection state**
   - Gray out drive widgets when FujiNet disconnected
   - Show overlay text: "FujiNet Disconnected"
   - Disable drive operations when unreachable
   - Auto-reconnect when FujiNet becomes available
   - Show reconnection spinner in status bar

4. **Status bar integration**
   - Show drive mode in status bar:
     - "Drives: FujiNet (NetSIO enabled)"
     - "Drives: Local"
   - Show operation feedback:
     - Copying: "Copying disk.atr to FujiNet... 45%"
     - Mounting: "Mounting D1..."
     - Success: "D1: disk.atr mounted (R/W)" (show for 3 seconds)
     - Eject: "D1: ejected" (show for 2 seconds)
     - Error: "Failed to mount D1: [error message]" (5 seconds, red text)
   - Connect to FujiNetService signals for status messages

### Phase 4: Settings Integration

**File**: `src/settingsdialog.cpp`, `include/settingsdialog.h`

1. **Reuse existing FujiNet SD path setting**
   - Use `m_fujinetSDPath` from Settings dialog
   - Pass this path to FujiNetService for file copy operations
   - Validate path exists before copying files
   - Add getter: `QString getFujiNetSDPath()` to share with MainWindow

2. **Share FujiNetService instance**
   - Instead of creating separate instances, create in MainWindow
   - Pass reference to Settings dialog
   - Both MainWindow and Settings use same instance for consistency

3. **Connection management**
   - Reuse existing FujiNet health check from Settings
   - Share connection status between Settings and MainWindow
   - Show FujiNet connection status in status bar

### Phase 5: Visual Feedback & Error Handling

1. **Drive activity LEDs**
   - Blink green LED when drive status updates show activity
   - Red LED for errors (mount failed, connection lost)
   - Add method: `void showActivity(bool read, bool write, int durationMs = 500)`
   - LED flash duration: ~500ms

2. **File copy progress indication**
   - Show spinner overlay on drive widget during copy (for all file sizes)
   - Status bar shows percentage for files > 1MB
   - Spinner implementation:
     - Semi-transparent overlay on drive widget
     - Centered QProgressIndicator or spinner animation
     - Disable drag-drop during copy operation

3. **Visual mode indicators**
   - Subtle blue background tint on drive widgets in FujiNet mode:
     ```cpp
     // In DiskDriveWidget::setDriveMode()
     if (mode == FUJINET) {
         setStyleSheet("background-color: rgba(100, 150, 255, 30);");
     } else {
         setStyleSheet("");
     }
     ```
   - Status bar text showing current mode

4. **Connection state visuals**
   - When FujiNet disconnected:
     - Gray out all drive widgets
     - Show overlay: "FujiNet Disconnected"
     - Disable all drive operations
   - During reconnection:
     - Small spinner in status bar
     - Status text: "Reconnecting to FujiNet..."

5. **Error handling scenarios**
   - **FujiNet not running**:
     - Gray out drives
     - Status bar: "FujiNet not available - drives in local mode"
     - Don't attempt operations

   - **File copy fails**:
     - Show error dialog with details
     - Revert drive to previous state
     - Don't attempt mount

   - **Mount fails**:
     - Show error dialog: "Failed to mount disk to FujiNet: [error]"
     - Revert drive to empty state
     - File remains in SD folder for manual retry

   - **Network timeout**:
     - Retry with exponential backoff (1s, 2s, 4s)
     - Show "Retrying..." in status bar
     - After 3 retries, show error dialog

## Files to Modify

### Core Implementation
- `include/fujinetservice.h` - Add drive status structs, parsing methods, file copy
- `src/fujinetservice.cpp` - Implement HTML parsing, file copy with progress, polling
- `include/diskdrivewidget.h` - Add FujiNet mode enum and methods
- `src/diskdrivewidget.cpp` - Implement FujiNet display/operations, spinner overlay
- `include/mainwindow.h` - Add FujiNetService member, drive mode tracking
- `src/mainwindow.cpp` - Wire up NetSIO state, status bar messages, drive updates

### Settings Integration
- `include/settingsdialog.h` - Add getter for SD path, share service instance
- `src/settingsdialog.cpp` - Share FujiNetService with MainWindow

### Documentation
- This file documents the plan and implementation details

## Key Technical Decisions

1. **Drive Mode Switching**: Automatic based on NetSIO enabled state
2. **Mount Strategy**: Copy to SD folder then mount (works with current FujiNet-PC)
3. **Access Mode**: Always mount as read-write (`mode=w`)
4. **Polling Rate**: Every 2-3 seconds when connected
5. **Host Number**: Always use host=1 (SD card)
6. **File Paths**: SD-relative paths (e.g., `/mydisk.atr`)
7. **SD Path**: Reuse existing `m_fujinetSDPath` setting from Settings
8. **HTML Parsing**: Use QRegularExpression to extract `data-mount` attributes
9. **Progress Feedback**: Spinner overlay on widget + status bar for large files
10. **Error Handling**: User dialogs for failures, automatic retry for network issues

## Implementation Order

1. **Phase 1** - FujiNetService enhancements (HTML parsing, file copy, fixed API calls)
2. **Phase 2** - DiskDriveWidget mode support (visual changes, FujiNet operations)
3. **Phase 3** - MainWindow integration (mode switching, signal connections)
4. **Phase 4** - Settings integration (share service, SD path)
5. **Phase 5** - Visual polish (LEDs, progress, error handling)

## Testing Plan

### Manual Testing
1. Start FujiNet-PC with SD folder configured
2. Enable NetSIO in Fujisan settings
3. Verify drives switch to FujiNet mode (blue tint, status bar update)
4. Drag .atr file to D1
5. Verify file copy progress shown
6. Verify file appears in FujiNet SD folder
7. Verify mount API called correctly
8. Verify drive shows mounted state
9. Verify eject works (unmount API)
10. Disable NetSIO, verify drives switch back to local mode

### Error Testing
1. FujiNet not running - verify graceful degradation
2. Invalid SD path - verify error message
3. File copy failure - verify error handling
4. Mount API failure - verify error handling
5. Network interruption - verify reconnection

## Future Enhancements

Not included in this implementation, but possible future work:

1. **Cassette Integration** - Similar pattern for cassette device
2. **Printer Integration** - Show printer status, access output
3. **FujiNet-PC API Enhancement** - Contribute direct path mounting support
4. **Real-time Activity** - WebSocket for live drive activity vs polling
5. **Network Hosts** - Browse and mount from fujinet.online (hosts 2-8)
6. **Disk Image Creation** - Create new .atr files directly from Fujisan
7. **Multi-disk Management** - Manage disk sets, auto-swap

## References

- FujiNet-PC Documentation: [docs/FUJINET_PC_TECHNICAL_ANALYSIS.md](FUJINET_PC_TECHNICAL_ANALYSIS.md)
- FujiNet Code Reference: [docs/FUJINET_PC_CODE_REFERENCE.md](FUJINET_PC_CODE_REFERENCE.md)
- TCP Server API: [docs/TCP_SERVER_API.md](TCP_SERVER_API.md)
- FujiNet-PC GitHub: https://github.com/FujiNetWIFI/fujinet-pc

## Notes

- This integration assumes FujiNet-PC is running locally on port 8000
- All disk operations in FujiNet mode use read-write access
- Local drive functionality remains unchanged when NetSIO is disabled
- The implementation maintains backward compatibility with non-FujiNet usage
