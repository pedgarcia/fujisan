# Fujisan Development Notes

## Real-Time Settings Changes

This document tracks which emulator settings can be changed on-the-fly without requiring a restart, and which ones require a restart. This is important for user experience and implementation planning.

### ✅ Real-Time Changes (No Restart Required)

#### **Color Adjustments**
- **PAL Color Settings**: Saturation, Contrast, Brightness, Gamma, Tint
- **NTSC Color Settings**: Saturation, Contrast, Brightness, Gamma, Tint
- **Implementation**: Direct modification of `COLOURS_PAL_setup` and `COLOURS_NTSC_setup` structs
- **Method**: `AtariEmulator::updatePalColorSettings()` and `AtariEmulator::updateNtscColorSettings()`
- **Files**: `src/settingsdialog.cpp` (slider connections), `src/atariemulator.cpp` (color methods)
- **Technical**: Uses `Colours_Update()` to refresh palette immediately

#### **Audio Settings**
- **Volume Control**: 0-100% volume adjustment with immediate feedback
- **Audio Enable/Disable**: Mute/unmute functionality works instantly
- **Implementation**: Qt audio device volume control and start/stop
- **Methods**: `AtariEmulator::setVolume()` and `AtariEmulator::enableAudio()`
- **Files**: `src/settingsdialog.cpp` lines ~378-382 (enable/disable), ~407-410 (volume)
- **Technical**: `m_audioOutput->setVolume()` and `m_audioOutput->start()/stop()`

#### **Performance Settings**
- **Emulation Speed**: 0.5x to 10x speed multipliers
- **Turbo Mode**: Enable/disable unlimited speed mode
- **Implementation**: Direct modification of `Atari800_turbo_speed` and `Atari800_turbo` variables
- **Method**: `AtariEmulator::setEmulationSpeed()`
- **Files**: `src/settingsdialog.cpp` (slider connection), `src/atariemulator.cpp` (speed method)
- **Technical**: Atari800 emulator core respects these variables immediately

### ❌ Restart Required Changes

#### **Audio Format Settings**
- **Sample Rate**: 22kHz, 44kHz, 48kHz changes
- **Bit Depth**: 8-bit vs 16-bit audio format
- **Buffer/Latency**: Audio buffer size and latency settings
- **Reason**: Requires Qt audio device recreation with new format
- **Behavior**: Settings saved, applied on next emulator restart

#### **Machine Configuration**
- **Machine Type**: 400/800, XL, XE, 5200, etc.
- **Video System**: PAL vs NTSC mode
- **Memory Configuration**: RAM expansions, MapRAM, etc.
- **BASIC Enable/Disable**: Atari BASIC ROM loading
- **Reason**: Core emulator initialization parameters
- **Behavior**: Full emulator restart with new configuration

#### **Hardware Extensions**
- **80-Column Cards**: XEP80, Austin Franklin, Bit3
- **PBI Extensions**: 1400XL, 1450XLD, Proto80
- **Voice Synthesis**: Voicebox speech synthesis
- **Stereo POKEY**: Dual sound chip configuration
- **Reason**: Hardware emulation components need reinitialization
- **Behavior**: Applied during next emulator restart

#### **Media Loading**
- **Cartridge Loading**: ROM/cartridge file changes
- **Disk Images**: Floppy disk mounting (though mounting works, format changes need restart)
- **Cassette Images**: Tape image loading
- **Reason**: File system and device initialization
- **Behavior**: Files loaded immediately, but some format changes need restart

### Implementation Patterns

#### **Real-Time Updates Pattern**
```cpp
// 1. Connect GUI control to lambda function
connect(m_controlWidget, &ControlType::signalName, [this](ValueType value) {
    // 2. Update UI feedback (labels, etc.)
    updateUIFeedback(value);
    
    // 3. Call emulator method immediately
    if (m_emulator) {
        m_emulator->updateSetting(value);
    }
});

// 4. Emulator method modifies core variables directly
void AtariEmulator::updateSetting(ValueType value) {
    core_variable = convertValue(value);
    core_update_function(); // If needed
    qDebug() << "Setting updated to:" << value;
}
```

#### **Restart Required Pattern**
```cpp
// 1. Settings are saved to QSettings
void SettingsDialog::saveSettings() {
    settings.setValue("category/setting", m_control->value());
}

// 2. Applied during emulator initialization
bool AtariEmulator::initializeWithConfig(...) {
    QSettings settings;
    ValueType value = settings.value("category/setting", defaultValue);
    // Configuration applied during startup
}
```

### Future Enhancement Opportunities

#### **Potential Real-Time Candidates**
- **Joystick Configuration**: Enable/disable, hat usage (if core supports)
- **SIO Acceleration**: Serial I/O speed changes (if core supports)
- **Display Artifacts**: NTSC artifacting modes (if core supports)
- **Mouse Settings**: Grab mode, device selection (if core supports)

#### **Investigation Required**
- **Stereo POKEY**: May be possible if core supports runtime enable/disable
- **Console Sounds**: Keyboard clicks, boot beeps (may be runtime changeable)
- **Serial Sounds**: SIO device sounds (may be runtime changeable)

### User Experience Guidelines

#### **Real-Time Changes**
- Provide immediate visual/audio feedback
- Update UI labels and displays instantly
- Use sliders and checkboxes for continuous adjustment
- Show percentage/value feedback for numerical settings

#### **Restart Required Changes**
- Clearly indicate restart requirement in UI
- Group related restart-required settings together
- Consider "Apply and Restart" button for these settings
- Preserve settings across restarts automatically

### Technical Notes

#### **Qt Audio Device Management**
- Volume changes: Use `QAudioOutput::setVolume()`
- Enable/disable: Use `QAudioOutput::start()/stop()`
- Format changes: Require device recreation with `new QAudioOutput(newFormat)`

#### **Atari800 Core Integration**
- Color system: Uses `COLOURS_NTSC_setup`/`COLOURS_PAL_setup` structs + `Colours_Update()`
- Speed control: Uses `Atari800_turbo_speed` and `Atari800_turbo` variables
- Machine config: Requires `libatari800_init()` with new arguments

#### **Settings Persistence**
- All settings use QSettings for automatic save/load
- Real-time settings update both emulator and settings storage
- Restart-required settings save to storage, load on next init

### Development History

- **Color Adjustments**: Implemented 2025-01 - Real-time PAL/NTSC color controls
- **Speed Control**: Implemented 2025-01 - Real-time emulation speed (0.5x-10x)  
- **Audio Volume/Enable**: Implemented 2025-01 - Real-time volume and mute controls
- **Spacing Improvements**: Implemented 2025-01 - UI layout and visual organization

---

*Last Updated: January 2025*
*Fujisan Version: Development*
*Atari800 Core: Latest*