/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "atariemulator.h"
#include <QDebug>
#include <QApplication>
#include <QMetaObject>
#include <QTimer>
#include <QFileInfo>

extern "C" {
#ifdef NETSIO
#include "../src/netsio.h"
#endif
#include "../src/rtime.h"
#include "../src/binload.h"
}

// Static callback function for libatari800 disk activity
static AtariEmulator* s_emulatorInstance = nullptr;
static void diskActivityCallback(int drive, int operation) {
    if (s_emulatorInstance) {
        // Convert libatari800 operation to Qt signal parameters
        bool isWriting = (operation == 1);  // SIO_LAST_WRITE = 1, SIO_LAST_READ = 0
        
        
        // Emit Qt signal on the main thread
        QMetaObject::invokeMethod(s_emulatorInstance, "diskIOStart", Qt::QueuedConnection,
                                Q_ARG(int, drive), Q_ARG(bool, isWriting));
        
        // Set a timer to turn the LED off after a short time (hardware-accurate timing)
        QTimer::singleShot(100, s_emulatorInstance, [drive]() {
            QMetaObject::invokeMethod(s_emulatorInstance, "diskIOEnd", Qt::QueuedConnection,
                                    Q_ARG(int, drive));
        });
    }
}

AtariEmulator::AtariEmulator(QObject *parent)
    : QObject(parent)
    , m_frameTimer(new QTimer(this))
    , m_audioOutput(nullptr)
    , m_audioDevice(nullptr)
    , m_audioEnabled(true)
    , m_fujinet_restart_pending(false)
    , m_fujinet_restart_delay(0)
{
    libatari800_clear_input_array(&m_currentInput);
    
    // Initialize joysticks to center position (15 = all directions released)
    m_currentInput.joy0 = 15;
    m_currentInput.joy1 = 15;
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
    
    // Set up the global instance pointer for the callback
    s_emulatorInstance = this;
    
    connect(m_frameTimer, &QTimer::timeout, this, &AtariEmulator::processFrame);
}

AtariEmulator::~AtariEmulator()
{
    // Clear the global instance pointer
    if (s_emulatorInstance == this) {
        s_emulatorInstance = nullptr;
        libatari800_set_disk_activity_callback(nullptr);
    }
    shutdown();
}

bool AtariEmulator::initialize()
{
    return initializeWithBasic(m_basicEnabled);
}

bool AtariEmulator::initializeWithBasic(bool basicEnabled)
{
    return initializeWithConfig(basicEnabled, m_machineType, m_videoSystem);
}

bool AtariEmulator::initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem)
{
    return initializeWithConfig(basicEnabled, machineType, videoSystem, "none");
}

bool AtariEmulator::initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode)
{
    // Use default display settings
    return initializeWithDisplayConfig(basicEnabled, machineType, videoSystem, artifactMode,
                                     "tv", "tv", 0, 0, "both", false, false);
}

bool AtariEmulator::initializeWithDisplayConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                               const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                               const QString& fitScreen, bool show80Column, bool vSyncEnabled)
{
    // FUTURE: Scanlines support would go here when working
    // return initializeWithConfig(basicEnabled, machineType, videoSystem, artifactMode, 0, false);
    
    // Current implementation without scanlines:
    m_basicEnabled = basicEnabled;
    m_machineType = machineType;
    m_videoSystem = videoSystem;
    
    // Build argument list with machine type, video system, artifacts, audio, and BASIC setting
    QStringList argList;
    argList << "atari800";
    argList << machineType;    // -xl, -xe, -atari, -5200, etc.
    argList << videoSystem;    // -ntsc or -pal
    
    // Add artifact settings - only supported modes for current build
    qDebug() << "Artifact mode requested:" << artifactMode;
    if (artifactMode != "none") {
        if (videoSystem == "-ntsc") {
            // Only ntsc-old and ntsc-new are supported (ntsc-full disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-ntsc-artif" << artifactMode;
                qDebug() << "Adding NTSC artifact parameters:" << "-ntsc-artif" << artifactMode;
            }
        } else if (videoSystem == "-pal") {
            // Map NTSC artifact modes to PAL simple (pal-blend disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-pal-artif" << "pal-simple";
                qDebug() << "Adding PAL artifact parameters:" << "-pal-artif" << "pal-simple";
            }
        }
    }
    
    // FUTURE: Scanlines support (commented out - parameters don't work with current build)
    // Add scanline settings - requires investigation of correct atari800 scanline support
    // May need different parameters, specific build flags, or SDL2 backend
    //
    // qDebug() << "Scanlines requested - Percentage:" << scanlinesPercentage << "Interpolation:" << scanlinesInterpolation;
    // if (scanlinesPercentage > 0) {
    //     argList << "-scanlines" << QString::number(scanlinesPercentage);
    //     qDebug() << "Adding scanlines parameter:" << "-scanlines" << scanlinesPercentage;
    //     if (scanlinesInterpolation) {
    //         argList << "-scanlinesint";
    //         qDebug() << "Adding scanlines interpolation parameter: -scanlinesint";
    //     }
    // }
    
    // LIMITATION: Display configuration parameters are NOT supported by libatari800
    // These parameters are only available in the full SDL build of atari800, not in the minimal libatari800 library.
    // The UI controls are preserved for future compatibility when we migrate to full SDL build.
    qDebug() << "=== SCREEN DISPLAY CONFIGURATION ===";
    qDebug() << "Display settings - HArea:" << horizontalArea << "VArea:" << verticalArea 
             << "HShift:" << horizontalShift << "VShift:" << verticalShift 
             << "Fit:" << fitScreen << "80Col:" << show80Column << "VSync:" << vSyncEnabled;
    qDebug() << "WARNING: libatari800 does not support display parameters - controls saved but not applied to emulator";
    
    // TODO: Display parameters - commented out because libatari800 doesn't support them
    // These would work with full SDL atari800 build:
    // argList << "-horiz-area" << horizontalArea;
    // argList << "-vert-area" << verticalArea;
    // if (horizontalShift != 0) argList << "-horiz-shift" << QString::number(horizontalShift);
    // if (verticalShift != 0) argList << "-vert-shift" << QString::number(verticalShift);
    // argList << "-fit-screen" << fitScreen;
    // if (show80Column) argList << "-80column"; else argList << "-no-80column";
    // if (vSyncEnabled) argList << "-vsync"; else argList << "-no-vsync";
    
    qDebug() << "=== END SCREEN DISPLAY CONFIGURATION ===";
    
    // Add audio configuration
    if (m_audioEnabled) {
        argList << "-sound";
        argList << "-dsprate" << "44100";
        argList << "-audio16";
        argList << "-volume" << "80";
        argList << "-speaker";  // Enable console speaker (keyboard clicks, boot beeps)
    } else {
        argList << "-nosound";
    }
    
    qDebug() << "Altirra OS enabled:" << m_altirraOSEnabled;
    
    if (m_altirraOSEnabled) {
        // Use built-in Altirra ROMs
        qDebug() << "Using Altirra OS ROMs";
        if (machineType == "-5200") {
            argList << "-5200-rev" << "altirra";
        } else if (machineType == "-atari") {
            argList << "-800-rev" << "altirra";
        } else {
            argList << "-xl-rev" << "altirra";
        }
        
        if (basicEnabled) {
            argList << "-basic-rev" << "altirra";
            argList << "-basic";  // Actually enable BASIC mode
        } else {
            argList << "-nobasic";
        }
    } else {
        // Use original Atari ROMs from user-specified paths or defaults
        qDebug() << "Using original Atari OS ROMs from user settings";
        
        // Only add ROM paths if they are specified in settings
        if (!m_osRomPath.isEmpty()) {
            if (machineType == "-5200") {
                argList << "-5200_rom" << m_osRomPath;
            } else {
                argList << "-xlxe_rom" << m_osRomPath;
            }
        } else {
            qDebug() << "Warning: No OS ROM path specified, emulator may not start properly";
        }
        
        if (basicEnabled) {
            if (!m_basicRomPath.isEmpty()) {
                argList << "-basic_rom" << m_basicRomPath;
            } else {
                qDebug() << "Warning: No BASIC ROM path specified";
            }
            argList << "-basic";  // Actually enable BASIC mode
        } else {
            argList << "-nobasic";
        }
    }
    
    // Convert QStringList to char* array
    QList<QByteArray> argBytes;
    for (const QString& arg : argList) {
        argBytes.append(arg.toUtf8());
    }
    
    char* args[argBytes.size() + 1];
    for (int i = 0; i < argBytes.size(); ++i) {
        args[i] = argBytes[i].data();
    }
    args[argBytes.size()] = nullptr;
    
    qDebug() << "=== COMPLETE COMMAND LINE ===";
    qDebug() << "Full atari800 command line:" << argList.join(" ");
    qDebug() << "Number of arguments:" << argBytes.size();
    qDebug() << "Note: Display parameters excluded due to libatari800 limitations";
    
    if (libatari800_init(argBytes.size(), args)) {
        qDebug() << "✓ Emulator initialized successfully";
        qDebug() << "  Display settings saved to profile but not applied (requires full SDL atari800)";
        m_targetFps = libatari800_get_fps();
        m_frameTimeMs = 1000.0f / m_targetFps;
        qDebug() << "Target FPS:" << m_targetFps << "Frame time:" << m_frameTimeMs << "ms";
        
        // Set up the disk activity callback for hardware-level monitoring
        libatari800_set_disk_activity_callback(diskActivityCallback);
        qDebug() << "✓ Disk activity callback registered with libatari800";
        
        // Debug SIO patch status to investigate disk speed changes
        debugSIOPatchStatus();
        
        // Initialize audio output if enabled
        if (m_audioEnabled) {
            setupAudio();
        }
        
        // Start the frame timer
        m_frameTimer->start(static_cast<int>(m_frameTimeMs));
        return true;
    }
    
    qDebug() << "✗ Failed to initialize emulator with:" << argList.join(" ");
    return false;
}

bool AtariEmulator::initializeWithInputConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                            const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                            const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                            bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks, bool netSIOEnabled)
{
    qDebug() << "Initializing emulator with input config - Joy0:" << kbdJoy0Enabled << "Joy1:" << kbdJoy1Enabled << "Swap:" << swapJoysticks;
    
    // Store display system and input configuration
    m_machineType = machineType;
    m_videoSystem = videoSystem;
    m_kbdJoy0Enabled = kbdJoy0Enabled;
    m_kbdJoy1Enabled = kbdJoy1Enabled;
    m_swapJoysticks = swapJoysticks;
    
    // Build argument list with machine type, video system, artifacts, audio, BASIC setting, and input settings
    QStringList argList;
    argList << "atari800";
    argList << machineType;    // -xl, -xe, -atari, -5200, etc.
    argList << videoSystem;    // -ntsc or -pal
    
    // Add artifact settings - only supported modes for current build
    qDebug() << "Artifact mode requested:" << artifactMode;
    if (artifactMode != "none") {
        if (videoSystem == "-ntsc") {
            // Only ntsc-old and ntsc-new are supported (ntsc-full disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-ntsc-artif" << artifactMode;
                qDebug() << "Adding NTSC artifact parameters:" << "-ntsc-artif" << artifactMode;
            }
        } else if (videoSystem == "-pal") {
            // Map NTSC artifact modes to PAL simple (pal-blend disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-pal-artif" << "pal-simple";
                qDebug() << "Adding PAL artifact parameters:" << "-pal-artif" << "pal-simple";
            }
        }
    }
    
    // Disable libatari800's internal keyboard joystick emulation
    // We handle joystick emulation ourselves via input_template_t.joy0/joy1
    qDebug() << "=== KEYBOARD JOYSTICK CONFIGURATION ===" ;
    argList << "-no-kbdjoy0";
    argList << "-no-kbdjoy1";
    qDebug() << "Disabled libatari800 internal keyboard joystick - using manual joystick control";
    
    // Audio is enabled by default for libatari800
    qDebug() << "Audio enabled for emulator";
    
    // Add OS and BASIC ROM configuration based on Altirra OS setting
    if (m_altirraOSEnabled) {
        qDebug() << "Using built-in Altirra OS ROMs";
        
        // Use built-in Altirra OS
        if (machineType == "-xl") {
            // For 800XL, explicitly set rev. 2 for better compatibility
            argList << "-xl-rev" << "2";
        } else {
            argList << "-xl-rev" << "altirra";
        }
        
        if (basicEnabled) {
            argList << "-basic-rev" << "altirra";
            argList << "-basic";  // Actually enable BASIC mode
        } else {
            argList << "-nobasic";
        }
    } else {
        // Use original Atari ROMs from user-specified paths or defaults
        qDebug() << "Using original Atari OS ROMs from user settings";
        
        if (!m_osRomPath.isEmpty()) {
            argList << "-osrom" << m_osRomPath;
            qDebug() << "Adding OS ROM path:" << m_osRomPath;
        } else {
            qDebug() << "Warning: No OS ROM path specified, emulator may not start properly";
        }
        
        if (basicEnabled) {
            if (!m_basicRomPath.isEmpty()) {
                argList << "-basic_rom" << m_basicRomPath;
            } else {
                qDebug() << "Warning: No BASIC ROM path specified";
            }
            argList << "-basic";  // Actually enable BASIC mode
        } else {
            argList << "-nobasic";
        }
    }
    
    // Add NetSIO support if enabled
    if (netSIOEnabled) {
        argList << "-netsio";
        qDebug() << "Adding NetSIO command line argument: -netsio";
    }
    
    // Convert QStringList to char* array
    QList<QByteArray> argBytes;
    for (const QString& arg : argList) {
        argBytes.append(arg.toUtf8());
    }
    
    char* args[argBytes.size() + 1];
    for (int i = 0; i < argBytes.size(); ++i) {
        args[i] = argBytes[i].data();
    }
    args[argBytes.size()] = nullptr;
    
    qDebug() << "=== COMPLETE COMMAND LINE WITH INPUT SETTINGS ===" ;
    qDebug() << "Full atari800 command line:" << argList.join(" ");
    qDebug() << "Number of arguments:" << argBytes.size();
    qDebug() << "Keyboard Joystick 0 (Numpad + RCtrl):" << (kbdJoy0Enabled ? "ENABLED" : "DISABLED");
    qDebug() << "Keyboard Joystick 1 (WASD + LCtrl):" << (kbdJoy1Enabled ? "ENABLED" : "DISABLED");
    
    if (libatari800_init(argBytes.size(), args)) {
        qDebug() << "✓ Emulator initialized successfully with input settings";
        m_targetFps = libatari800_get_fps();
        m_frameTimeMs = 1000.0f / m_targetFps;
        qDebug() << "Target FPS:" << m_targetFps << "Frame time:" << m_frameTimeMs << "ms";
        
        // Set up the disk activity callback for hardware-level monitoring
        libatari800_set_disk_activity_callback(diskActivityCallback);
        qDebug() << "✓ Disk activity callback registered with libatari800";
        
        // Initialize audio output if enabled
        if (m_audioEnabled) {
            setupAudio();
        }
        
        // Start the frame timer
        m_frameTimer->start(static_cast<int>(m_frameTimeMs));
        return true;
    }
    
    qDebug() << "✗ Failed to initialize emulator with input settings:" << argList.join(" ");
    return false;
}

bool AtariEmulator::initializeWithNetSIOConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                             const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                             const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                             bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks,
                                             bool netSIOEnabled, bool rtimeEnabled)
{
    qDebug() << "Initializing emulator with NetSIO config - NetSIO:" << netSIOEnabled << "RTime:" << rtimeEnabled;
    
    // CRITICAL: NetSIO/FujiNet requires BASIC to be disabled for proper booting
    bool actualBasicEnabled = basicEnabled;
    if (netSIOEnabled && basicEnabled) {
        actualBasicEnabled = false;
        // Update the emulator's internal BASIC state to reflect the auto-disable
        setBasicEnabled(false);
        qDebug() << "*** AUTOMATICALLY DISABLING BASIC FOR FUJINET ***";
        qDebug() << "NetSIO/FujiNet requires BASIC disabled to boot from network devices";
        qDebug() << "Original BASIC setting:" << basicEnabled << "-> Using:" << actualBasicEnabled;
    } else {
        // Ensure emulator's internal state matches the requested setting when NetSIO is disabled
        setBasicEnabled(basicEnabled);
    }
    
    // Start with basic initialization first
    if (!initializeWithInputConfig(actualBasicEnabled, machineType, videoSystem, artifactMode,
                                 horizontalArea, verticalArea, horizontalShift, verticalShift,
                                 fitScreen, show80Column, vSyncEnabled,
                                 kbdJoy0Enabled, kbdJoy1Enabled, swapJoysticks, netSIOEnabled)) {
        return false;
    }
    
    // Debug SIO patch status after initialization (for NetSIO investigation)
    debugSIOPatchStatus();
    
    // NetSIO is now initialized by atari800 core via -netsio command line argument
    if (netSIOEnabled) {
        qDebug() << "NetSIO enabled via command line argument - setting up delayed restart mechanism";
        
        // CRITICAL: Dismount all local disks to give FujiNet boot priority
        // When NetSIO is enabled, FujiNet devices should take precedence over local ATR files
        qDebug() << "Dismounting local disks to give FujiNet boot priority";
        for (int i = 1; i <= 8; i++) {
            dismountDiskImage(i);
        }
        
        // Use delayed restart mechanism like Atari800MacX for proper FujiNet timing
        m_fujinet_restart_pending = true;
        m_fujinet_restart_delay = 60; // Wait 60 frames (~1 second) 
        qDebug() << "FujiNet delayed restart set: 60 frames";
        
        // Send test command to verify NetSIO communication with FujiNet-PC
#ifdef NETSIO
        extern void netsio_test_cmd(void);
        netsio_test_cmd();
        qDebug() << "NetSIO test command sent to FujiNet-PC";
#endif
    }
    
    // Enable R-Time 8 if requested
    if (rtimeEnabled) {
        qDebug() << "Enabling R-Time 8 real-time clock...";
        extern int RTIME_enabled;
        RTIME_enabled = 1;
    }
    
    return true;
}

void AtariEmulator::shutdown()
{
    m_frameTimer->stop();
    libatari800_exit();
}

void AtariEmulator::processFrame()
{
    // Handle delayed FujiNet restart (critical for FujiNet timing)
    if (m_fujinet_restart_pending && m_fujinet_restart_delay > 0) {
        m_fujinet_restart_delay--;
        if (m_fujinet_restart_delay == 10) { // Debug at 10 frames remaining
            qDebug() << "FujiNet delayed restart countdown:" << m_fujinet_restart_delay << "frames remaining";
        }
        if (m_fujinet_restart_delay == 0) {
            qDebug() << "Triggering delayed FujiNet machine initialization";
            extern int Atari800_InitialiseMachine(void);
            Atari800_InitialiseMachine();
            m_fujinet_restart_pending = false;
            qDebug() << "FujiNet delayed machine initialization completed";
        }
    }

    // Debug joystick values being sent to libatari800
    static int lastJoy0 = -1, lastJoy1 = -1;
    if (m_currentInput.joy0 != lastJoy0 || m_currentInput.joy1 != lastJoy1) {
        qDebug() << "*** SENDING TO LIBATARI800: Joy0=" << m_currentInput.joy0 << "Joy1=" << m_currentInput.joy1 
                 << "Trig0=" << m_currentInput.trig0 << "Trig1=" << m_currentInput.trig1 << "***";
        lastJoy0 = m_currentInput.joy0;
        lastJoy1 = m_currentInput.joy1;
    }
    
    // Debug what we're sending to the emulator
    if (m_currentInput.keychar != 0) {
        // qDebug() << "*** SENDING TO EMULATOR: keychar=" << (int)m_currentInput.keychar << "'" << QChar(m_currentInput.keychar) << "' ***";
    }
    // Always debug L key specifically since AKEY_l = 0
    bool hasInput = m_currentInput.keychar != 0 || m_currentInput.keycode != 0 || m_currentInput.special != 0 ||
                   m_currentInput.start != 0 || m_currentInput.select != 0 || m_currentInput.option != 0;
    
    if (hasInput) {
        // qDebug() << "*** SENDING TO EMULATOR: keychar=" << (int)m_currentInput.keychar 
        //          << " keycode=" << (int)m_currentInput.keycode 
        //          << " special=" << (int)m_currentInput.special 
        //          << " start=" << (int)m_currentInput.start
        //          << " select=" << (int)m_currentInput.select
        //          << " option=" << (int)m_currentInput.option << " ***";
    }
    
    libatari800_next_frame(&m_currentInput);
    
    // Disk I/O monitoring is now handled by libatari800 callback
    
    // Handle audio output
    if (m_audioEnabled && m_audioOutput && m_audioDevice) {
        unsigned char* soundBuffer = libatari800_get_sound_buffer();
        int soundBufferLen = libatari800_get_sound_buffer_len();
        
        if (soundBuffer && soundBufferLen > 0) {
            qint64 bytesWritten = m_audioDevice->write(reinterpret_cast<const char*>(soundBuffer), soundBufferLen);
            // Only log incomplete writes if they're significantly incomplete (less than 90%)
            if (bytesWritten < soundBufferLen * 0.9) {
                static int underrunCount = 0;
                underrunCount++;
                if (underrunCount % 100 == 1) { // Log every 100th underrun to reduce spam
                    qDebug() << "Audio underrun #" << underrunCount << ":" << bytesWritten << "of" << soundBufferLen << "bytes";
                }
            }
        }
    }
    
    // Don't clear input here - let it persist until key release
    
    emit frameReady();
}

const unsigned char* AtariEmulator::getScreen()
{
    return libatari800_get_screen_ptr();
}

bool AtariEmulator::loadFile(const QString& filename)
{
    // Determine file type by extension
    QFileInfo fileInfo(filename);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "xex" || extension == "exe" || extension == "com") {
        // Load XEX/EXE/COM files as executables using BINLOAD
        qDebug() << "Loading executable file:" << filename;
        
        int result = BINLOAD_Loader(filename.toUtf8().constData());
        
        if (result) {
            qDebug() << "Successfully loaded and executed:" << filename;
            return true;
        } else {
            qDebug() << "Failed to load executable:" << filename;
            return false;
        }
    } else {
        // Load other files (CAR, ROM, etc.) as cartridges
        qDebug() << "Loading ROM/cartridge:" << filename;
        
        // First, explicitly remove any existing cartridge with reboot
        // This ensures a clean slate before loading the new cartridge
        CARTRIDGE_RemoveAutoReboot();
        qDebug() << "Removed existing cartridge";
        
        // Now insert the new cartridge with auto-reboot
        // This provides a complete system reset with the new cartridge
        int result = CARTRIDGE_InsertAutoReboot(filename.toUtf8().constData());
        
        if (result) {
            qDebug() << "Successfully loaded cartridge:" << filename;
            return true;
        } else {
            qDebug() << "Failed to load cartridge:" << filename;
            // Fall back to the original method for non-cartridge files
            qDebug() << "Trying fallback method with libatari800_reboot_with_file";
            bool fallback_result = libatari800_reboot_with_file(filename.toUtf8().constData());
            if (fallback_result) {
                qDebug() << "Fallback method succeeded for:" << filename;
            }
            return fallback_result;
        }
    }
}

bool AtariEmulator::mountDiskImage(int driveNumber, const QString& filename, bool readOnly)
{
    // Validate drive number (1-8 for D1: through D8:)
    if (driveNumber < 1 || driveNumber > 8) {
        qDebug() << "Invalid drive number:" << driveNumber << "- must be 1-8";
        return false;
    }
    
    if (filename.isEmpty()) {
        qDebug() << "Empty filename provided for drive D" << driveNumber << ":";
        return false;
    }
    
    qDebug() << "Attempting to mount" << filename << "to drive D" << driveNumber << ":";
    
    // Mount the disk image using libatari800
    int result = libatari800_mount_disk_image(driveNumber, filename.toUtf8().constData(), readOnly ? 1 : 0);
    
    qDebug() << "libatari800_mount_disk_image returned:" << result;
    
    if (result) {
        // Store the path for tracking
        m_diskImages[driveNumber - 1] = filename;
        m_mountedDrives.insert(driveNumber);
        
        qDebug() << "Successfully mounted" << filename << "to drive D" << driveNumber << ":" 
                 << (readOnly ? "(read-only)" : "(read-write)");
        qDebug() << "Disk should now be accessible from the emulated Atari";
        qDebug() << "Disk activity LEDs will now be controlled by libatari800 callback";
        
        return true;
    } else {
        qDebug() << "Failed to mount" << filename << "to drive D" << driveNumber << ":";
        qDebug() << "Check if file exists and is a valid Atari disk image";
        return false;
    }
}

void AtariEmulator::dismountDiskImage(int driveNumber)
{
    if (driveNumber < 1 || driveNumber > 8) {
        qWarning() << "Invalid drive number for dismount:" << driveNumber;
        return;
    }
    
    // Actually dismount from libatari800 core
    libatari800_dismount_disk_image(driveNumber);
    
    // Clear Fujisan internal state
    m_diskImages[driveNumber - 1].clear();
    m_mountedDrives.remove(driveNumber);
    qDebug() << "Dismounted drive D" << driveNumber << ": - libatari800 core and Fujisan state cleared";
}

void AtariEmulator::disableDrive(int driveNumber)
{
    if (driveNumber < 1 || driveNumber > 8) {
        qWarning() << "Invalid drive number for disable:" << driveNumber;
        return;
    }
    
    // Disable drive in libatari800 core (dismounts disk and sets status to OFF)
    libatari800_disable_drive(driveNumber);
    
    // Clear Fujisan internal state
    m_diskImages[driveNumber - 1].clear();
    m_mountedDrives.remove(driveNumber);
    qDebug() << "Disabled drive D" << driveNumber << ": - libatari800 core and Fujisan state cleared";
}

void AtariEmulator::coldRestart()
{
    // Trigger Atari800 cold start to refresh boot sequence
    Atari800_Coldstart();
    qDebug() << "Cold restart completed - boot sequence refreshed";
}

QString AtariEmulator::getDiskImagePath(int driveNumber) const
{
    // Validate drive number (1-8 for D1: through D8:)
    if (driveNumber < 1 || driveNumber > 8) {
        return QString();
    }
    
    return m_diskImages[driveNumber - 1];
}

void AtariEmulator::handleKeyPress(QKeyEvent* event)
{
    // Check for joystick keyboard emulation first
    if (handleJoystickKeyboardEmulation(event)) {
        return; // Key was handled by joystick emulation, don't process as regular key
    }
    
    // Don't clear input here - let keys persist across frames until released
    
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    bool shiftPressed = modifiers & Qt::ShiftModifier;
    bool ctrlPressed = modifiers & Qt::ControlModifier;
    bool metaPressed = modifiers & Qt::MetaModifier;
    
    // On macOS, support both Ctrl and Cmd keys for control codes
    bool controlKeyPressed = ctrlPressed || metaPressed;
    
    qDebug() << "Key pressed:" << key << "modifiers:" << modifiers 
             << "Ctrl:" << ctrlPressed << "Shift:" << shiftPressed
             << "Meta:" << metaPressed << "ControlKey:" << controlKeyPressed;
    
    // Handle Control key combinations using proper AKEY codes  
    // Build modifier bits like atari800 SDL does
    if (controlKeyPressed && key >= Qt::Key_A && key <= Qt::Key_Z) {
        unsigned char baseKey = convertQtKeyToAtari(key, Qt::NoModifier);
        if (baseKey != 0) {
            // Build shiftctrl modifier bits
            int shiftctrl = 0;
            if (shiftPressed) shiftctrl |= AKEY_SHFT;
            if (controlKeyPressed) shiftctrl |= AKEY_CTRL;
            
            m_currentInput.keycode = baseKey | shiftctrl;
            
            if (shiftPressed && ctrlPressed) {
                qDebug() << "Setting AKEY_SHFTCTRL keycode for" << QChar(key) << ":" << (int)m_currentInput.keycode
                         << "base:" << (int)baseKey << "shiftctrl:" << (int)shiftctrl << "(pure control)";
            } else {
                qDebug() << "Setting AKEY_CTRL keycode for" << QChar(key) << ":" << (int)m_currentInput.keycode
                         << "base:" << (int)baseKey << "shiftctrl:" << (int)shiftctrl << "(display control)";
            }
        } else {
            qDebug() << "Could not map" << QChar(key) << "to base AKEY";
        }
    } else if (key == Qt::Key_CapsLock) {
        // Send CAPS LOCK toggle to the emulator to change its internal state
        m_currentInput.keycode = AKEY_CAPSTOGGLE;
        qDebug() << "*** CAPS LOCK KEY PRESSED! Sending AKEY_CAPSTOGGLE to emulator ***";
    } else if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        if (key == Qt::Key_L) {
            // Special handling for L key since AKEY_l = 0 causes issues
            // Use keychar approach for L key only - send lowercase so emulator can apply case logic
            m_currentInput.keychar = 'l';  // Send lowercase, emulator handles case via CAPS LOCK
            qDebug() << "*** SPECIAL L KEY: Using keychar='l' (lowercase) instead of keycode=0 ***";
        } else {
            // Normal letter handling using keycode
            unsigned char baseKey = convertQtKeyToAtari(key, Qt::NoModifier);
            m_currentInput.keycode = baseKey;
            qDebug() << "Setting keycode to:" << (int)baseKey << "(letter - emulator handles case) Qt key:" << key << "=" << QChar(key);
        }
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        if (shiftPressed) {
            // Handle shifted number keys
            QString shiftedSymbols = ")!@#$%^&*(";
            int index = key - Qt::Key_0;
            if (index < shiftedSymbols.length()) {
                m_currentInput.keychar = shiftedSymbols[index].toLatin1();
                qDebug() << "*** SHIFTED NUMBER DETECTED! Key:" << key << "Index:" << index << "Setting keychar to:" << QChar(m_currentInput.keychar) << "(shifted) ***";
            }
        } else {
            m_currentInput.keychar = key - Qt::Key_0 + '0';
            qDebug() << "Setting keychar to:" << QChar(m_currentInput.keychar);
        }
    } else if (key == Qt::Key_Space) {
        m_currentInput.keychar = ' ';
        qDebug() << "Setting keychar to: SPACE";
    } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        // Use keycode approach for RETURN key
        m_currentInput.keycode = AKEY_RETURN; // Atari RETURN key code
        qDebug() << "*** ENTER KEY DETECTED! Setting keycode=" << (int)AKEY_RETURN << " ***";
    } else if (key == Qt::Key_F2) {
        // F2 = Start
        m_currentInput.start = 1;
        qDebug() << "*** F2 DETECTED! START pressed ***";
    } else if (key == Qt::Key_F3) {
        // F3 = Select
        m_currentInput.select = 1;
        qDebug() << "*** F3 DETECTED! SELECT pressed ***";
    } else if (key == Qt::Key_F4) {
        // F4 = Option
        m_currentInput.option = 1;
        qDebug() << "*** F4 DETECTED! OPTION pressed ***";
    } else if (key == Qt::Key_F5 && shiftPressed) {
        // Shift+F5 = Cold Reset (Power) - libatari800 does: lastkey = -input->special
        m_currentInput.special = -AKEY_COLDSTART;  // Convert -3 to +3
        qDebug() << "*** SHIFT+F5 DETECTED! Cold Reset (Power) - setting special to" << (int)m_currentInput.special << " ***";
    } else if (key == Qt::Key_F5 && !shiftPressed) {
        // F5 = Warm Reset - libatari800 does: lastkey = -input->special  
        m_currentInput.special = -AKEY_WARMSTART;  // Convert -2 to +2
        qDebug() << "*** F5 DETECTED! Warm Reset - setting special to" << (int)m_currentInput.special << " ***";
    } else if (key == Qt::Key_F6) {
        // F6 = Help
        m_currentInput.keycode = AKEY_HELP;
        qDebug() << "*** F6 DETECTED! HELP pressed ***";
    } else if (key == Qt::Key_F7 || key == Qt::Key_Pause) {
        // F7 or Pause = Break - libatari800 does: lastkey = -input->special
        m_currentInput.special = -AKEY_BREAK;  // Convert -5 to +5
        qDebug() << "*** BREAK KEY DETECTED! F7/Pause pressed - setting special to" << (int)m_currentInput.special << " ***";
    } else if (key == Qt::Key_Exclam) {
        m_currentInput.keychar = '!';
        qDebug() << "*** EXCLAMATION DETECTED! Setting keychar to: ! ***";
    } else if (key == Qt::Key_At) {
        m_currentInput.keychar = '@';
        qDebug() << "*** AT SYMBOL DETECTED! Setting keychar to: @ ***";
    } else if (key == Qt::Key_NumberSign) {
        m_currentInput.keychar = '#';
        qDebug() << "*** HASH DETECTED! Setting keychar to: # ***";
    } else if (key == Qt::Key_Dollar) {
        m_currentInput.keychar = '$';
        qDebug() << "*** DOLLAR DETECTED! Setting keychar to: $ ***";
    } else if (key == Qt::Key_Percent) {
        m_currentInput.keychar = '%';
        qDebug() << "*** PERCENT DETECTED! Setting keychar to: % ***";
    } else if (key == Qt::Key_AsciiCircum) {
        m_currentInput.keychar = '^';
        qDebug() << "*** CARET DETECTED! Setting keychar to: ^ ***";
    } else if (key == Qt::Key_Ampersand) {
        m_currentInput.keychar = '&';
        qDebug() << "*** AMPERSAND DETECTED! Setting keychar to: & ***";
    } else if (key == Qt::Key_Asterisk) {
        m_currentInput.keychar = '*';
        qDebug() << "*** ASTERISK DETECTED! Setting keychar to: * ***";
    } else if (key == Qt::Key_ParenLeft) {
        m_currentInput.keychar = '(';
        qDebug() << "*** PAREN LEFT DETECTED! Setting keychar to: ( ***";
    } else if (key == Qt::Key_ParenRight) {
        m_currentInput.keychar = ')';
        qDebug() << "*** PAREN RIGHT DETECTED! Setting keychar to: ) ***";
    } else if (key == Qt::Key_Question) {
        m_currentInput.keychar = '?';
        qDebug() << "*** QUESTION MARK DETECTED! Setting keychar to: ? ***";
    } else if (key == Qt::Key_Colon) {
        m_currentInput.keychar = ':';
        qDebug() << "*** COLON DETECTED! Setting keychar to: : ***";
    } else if (key == Qt::Key_Plus) {
        m_currentInput.keychar = '+';
        qDebug() << "*** PLUS DETECTED! Setting keychar to: + ***";
    } else if (key == Qt::Key_Less) {
        m_currentInput.keychar = '<';
        qDebug() << "*** LESS THAN DETECTED! Setting keychar to: < ***";
    } else if (key == Qt::Key_Underscore) {
        m_currentInput.keychar = '_';
        qDebug() << "*** UNDERSCORE DETECTED! Setting keychar to: _ ***";
    } else if (key == Qt::Key_Greater) {
        m_currentInput.keychar = '>';
        qDebug() << "*** GREATER THAN DETECTED! Setting keychar to: > ***";
    } else if (key == Qt::Key_QuoteDbl) {
        m_currentInput.keychar = '"';
        qDebug() << "*** QUOTE DETECTED! Setting keychar to: \" ***";
    } else if (key == Qt::Key_BraceLeft) {
        m_currentInput.keychar = '{';
        qDebug() << "*** BRACE LEFT DETECTED! Setting keychar to: { ***";
    } else if (key == Qt::Key_Bar) {
        m_currentInput.keychar = '|';
        qDebug() << "*** BAR DETECTED! Setting keychar to: | ***";
    } else if (key == Qt::Key_BraceRight) {
        m_currentInput.keychar = '}';
        qDebug() << "*** BRACE RIGHT DETECTED! Setting keychar to: } ***";
    } else if (key == Qt::Key_AsciiTilde) {
        m_currentInput.keychar = '~';
        qDebug() << "*** TILDE DETECTED! Setting keychar to: ~ ***";
    } else {
        // Handle other shifted symbols and regular punctuation
        char symbol = getShiftedSymbol(key, shiftPressed);
        if (symbol != 0) {
            m_currentInput.keychar = symbol;
            qDebug() << "Setting keychar to:" << QChar(symbol);
        } else {
            // Handle regular punctuation and special keys
            switch (key) {
                case Qt::Key_Semicolon:
                    m_currentInput.keychar = ';';
                    qDebug() << "Setting keychar to: ;";
                    break;
                case Qt::Key_Equal:
                    m_currentInput.keychar = '=';
                    qDebug() << "Setting keychar to: =";
                    break;
                case Qt::Key_Comma:
                    m_currentInput.keychar = ',';
                    qDebug() << "Setting keychar to: ,";
                    break;
                case Qt::Key_Minus:
                    m_currentInput.keychar = '-';
                    qDebug() << "Setting keychar to: -";
                    break;
                case Qt::Key_Period:
                    m_currentInput.keychar = '.';
                    qDebug() << "Setting keychar to: .";
                    break;
                case Qt::Key_Slash:
                    m_currentInput.keychar = '/';
                    qDebug() << "Setting keychar to: /";
                    break;
                case Qt::Key_Apostrophe:
                    m_currentInput.keychar = '\'';
                    qDebug() << "Setting keychar to: '";
                    break;
                case Qt::Key_QuoteLeft:
                    m_currentInput.keychar = '`';
                    qDebug() << "Setting keychar to: `";
                    break;
                case Qt::Key_BracketLeft:
                    m_currentInput.keychar = '[';
                    qDebug() << "Setting keychar to: [";
                    break;
                case Qt::Key_BracketRight:
                    m_currentInput.keychar = ']';
                    qDebug() << "Setting keychar to: ]";
                    break;
                case Qt::Key_Backslash:
                    m_currentInput.keychar = '\\';
                    qDebug() << "Setting keychar to: \\";
                    break;
                default:
                    // For special keys, use keycode
                    unsigned char atariKey = convertQtKeyToAtari(key, modifiers);
                    if (atariKey != 0) {
                        m_currentInput.keycode = atariKey;
                        qDebug() << "Setting keycode to:" << (int)atariKey;
                    }
                    break;
            }
        }
    }
}

void AtariEmulator::handleKeyRelease(QKeyEvent* event)
{
    // Check for joystick keyboard emulation first  
    if (handleJoystickKeyboardEmulation(event)) {
        return; // Key was handled by joystick emulation, don't clear all input
    }
    
    // Clear input when any key is released (for regular keyboard input)
    libatari800_clear_input_array(&m_currentInput);
    // Restore joystick center positions after clearing (using libatari800-compatible center)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.joy1 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
    qDebug() << "Key released - clearing input";
}

void AtariEmulator::coldBoot()
{
    Atari800_Coldstart();
    qDebug() << "Cold boot performed";
}

void AtariEmulator::warmBoot()
{
    Atari800_Warmstart();
    qDebug() << "Warm boot performed";
}

char AtariEmulator::getShiftedSymbol(int key, bool shiftPressed)
{
    switch (key) {
        case Qt::Key_Semicolon: return shiftPressed ? ':' : ';';
        case Qt::Key_Equal: return shiftPressed ? '+' : '=';
        case Qt::Key_Comma: return shiftPressed ? '<' : ',';
        case Qt::Key_Minus: return shiftPressed ? '_' : '-';
        case Qt::Key_Period: return shiftPressed ? '>' : '.';
        case Qt::Key_Slash: return shiftPressed ? '?' : '/';
        case Qt::Key_Apostrophe: return shiftPressed ? '"' : '\'';
        case Qt::Key_BracketLeft: return shiftPressed ? '{' : '[';
        case Qt::Key_Backslash: return shiftPressed ? '|' : '\\';
        case Qt::Key_BracketRight: return shiftPressed ? '}' : ']';
        case Qt::Key_QuoteLeft: return shiftPressed ? '~' : '`';
        default: return 0;
    }
}

unsigned char AtariEmulator::convertQtKeyToAtari(int key, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    
    switch (key) {
        // Letters - return base AKEY codes (lowercase)
        case Qt::Key_A: return AKEY_a;
        case Qt::Key_B: return AKEY_b;
        case Qt::Key_C: return AKEY_c;
        case Qt::Key_D: return AKEY_d;
        case Qt::Key_E: return AKEY_e;
        case Qt::Key_F: return AKEY_f;
        case Qt::Key_G: return AKEY_g;
        case Qt::Key_H: return AKEY_h;
        case Qt::Key_I: return AKEY_i;
        case Qt::Key_J: return AKEY_j;
        case Qt::Key_K: return AKEY_k;
        case Qt::Key_L: return AKEY_l;
        case Qt::Key_M: return AKEY_m;
        case Qt::Key_N: return AKEY_n;
        case Qt::Key_O: return AKEY_o;
        case Qt::Key_P: return AKEY_p;
        case Qt::Key_Q: return AKEY_q;
        case Qt::Key_R: return AKEY_r;
        case Qt::Key_S: return AKEY_s;
        case Qt::Key_T: return AKEY_t;
        case Qt::Key_U: return AKEY_u;
        case Qt::Key_V: return AKEY_v;
        case Qt::Key_W: return AKEY_w;
        case Qt::Key_X: return AKEY_x;
        case Qt::Key_Y: return AKEY_y;
        case Qt::Key_Z: return AKEY_z;
        
        // Special keys
        case Qt::Key_Escape: return AKEY_ESCAPE;
        case Qt::Key_Backspace: return AKEY_BACKSPACE;
        case Qt::Key_Tab: return AKEY_TAB;
        case Qt::Key_Up: return AKEY_UP;
        case Qt::Key_Down: return AKEY_DOWN;
        case Qt::Key_Left: return AKEY_LEFT;
        case Qt::Key_Right: return AKEY_RIGHT;
        case Qt::Key_F1: return AKEY_F1;
        default: return 0;
    }
}

void AtariEmulator::setupAudio()
{
    qDebug() << "Setting up audio output...";
    
    // Get audio parameters from libatari800
    int frequency = libatari800_get_sound_frequency();
    int channels = libatari800_get_num_sound_channels();
    int sampleSize = libatari800_get_sound_sample_size();
    
    qDebug() << "Audio config - Frequency:" << frequency << "Hz, Channels:" << channels << "Sample size:" << sampleSize << "bytes";
    
    // Setup Qt audio format
    QAudioFormat format;
    format.setSampleRate(frequency);
    format.setChannelCount(channels);
    format.setSampleSize(sampleSize * 8); // Convert bytes to bits
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(sampleSize == 2 ? QAudioFormat::SignedInt : QAudioFormat::UnSignedInt);
    
    // Check if format is supported
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
    if (!info.isFormatSupported(format)) {
        qDebug() << "Audio format not supported, trying to use nearest format";
        format = info.nearestFormat(format);
    }
    
    qDebug() << "Using audio format - Rate:" << format.sampleRate() << "Channels:" << format.channelCount() << "Sample size:" << format.sampleSize();
    
    // Create and start audio output
    m_audioOutput = new QAudioOutput(format, this);
    // Increase buffer size to reduce underruns (44100 Hz * 2 bytes * 2 channels * 0.1 sec = ~17KB)
    m_audioOutput->setBufferSize(16384); 
    m_audioDevice = m_audioOutput->start();
    
    if (m_audioDevice) {
        qDebug() << "Audio output started successfully";
    } else {
        qDebug() << "Failed to start audio output";
        m_audioEnabled = false;
    }
}

void AtariEmulator::enableAudio(bool enabled)
{
    if (m_audioEnabled != enabled) {
        m_audioEnabled = enabled;
        
        if (enabled && !m_audioOutput) {
            setupAudio();
        } else if (!enabled && m_audioOutput) {
            m_audioOutput->stop();
            delete m_audioOutput;
            m_audioOutput = nullptr;
            m_audioDevice = nullptr;
        }
        
        qDebug() << "Audio" << (enabled ? "enabled" : "disabled");
    }
}

void AtariEmulator::setVolume(float volume)
{
    if (m_audioOutput) {
        m_audioOutput->setVolume(qBound(0.0f, volume, 1.0f));
        qDebug() << "Audio volume set to:" << volume;
    }
}

void AtariEmulator::updateColorSettings(bool isPal, double saturation, double contrast, double brightness, double gamma, double hue)
{
    if (isPal) {
        updatePalColorSettings(saturation, contrast, brightness, gamma, hue);
    } else {
        updateNtscColorSettings(saturation, contrast, brightness, gamma, hue);
    }
}

void AtariEmulator::updatePalColorSettings(double saturation, double contrast, double brightness, double gamma, double hue)
{
    // Convert slider values to actual ranges
    COLOURS_PAL_setup.saturation = saturation / 100.0;           // -100 to 100 → -1.0 to 1.0
    COLOURS_PAL_setup.contrast = contrast / 100.0;               // -100 to 100 → -1.0 to 1.0  
    COLOURS_PAL_setup.brightness = brightness / 100.0;           // -100 to 100 → -1.0 to 1.0
    COLOURS_PAL_setup.gamma = gamma / 100.0;                     // 10 to 400 → 0.1 to 4.0
    COLOURS_PAL_setup.hue = hue / 180.0;                         // -180 to 180 → -1.0 to 1.0
    
    // Update the color palette
    Colours_Update();
    
    qDebug() << "PAL color settings updated - Sat:" << COLOURS_PAL_setup.saturation 
             << "Cont:" << COLOURS_PAL_setup.contrast
             << "Bright:" << COLOURS_PAL_setup.brightness
             << "Gamma:" << COLOURS_PAL_setup.gamma
             << "Hue:" << COLOURS_PAL_setup.hue;
}

void AtariEmulator::updateNtscColorSettings(double saturation, double contrast, double brightness, double gamma, double hue)
{
    // Convert slider values to actual ranges
    COLOURS_NTSC_setup.saturation = saturation / 100.0;           // -100 to 100 → -1.0 to 1.0
    COLOURS_NTSC_setup.contrast = contrast / 100.0;               // -100 to 100 → -1.0 to 1.0
    COLOURS_NTSC_setup.brightness = brightness / 100.0;           // -100 to 100 → -1.0 to 1.0
    COLOURS_NTSC_setup.gamma = gamma / 100.0;                     // 10 to 400 → 0.1 to 4.0
    COLOURS_NTSC_setup.hue = hue / 180.0;                         // -180 to 180 → -1.0 to 1.0
    
    // Update the color palette
    Colours_Update();
    
    qDebug() << "NTSC color settings updated - Sat:" << COLOURS_NTSC_setup.saturation 
             << "Cont:" << COLOURS_NTSC_setup.contrast
             << "Bright:" << COLOURS_NTSC_setup.brightness
             << "Gamma:" << COLOURS_NTSC_setup.gamma
             << "Hue:" << COLOURS_NTSC_setup.hue;
}

void AtariEmulator::updateArtifactSettings(const QString& artifactMode)
{
    qDebug() << "Updating artifact mode to:" << artifactMode;
    
    // Map string mode to ARTIFACT_t enum
    ARTIFACT_t mode = ARTIFACT_NONE;
    
    if (artifactMode == "none") {
        mode = ARTIFACT_NONE;
    } else if (artifactMode == "ntsc-old") {
        mode = ARTIFACT_NTSC_OLD;
    } else if (artifactMode == "ntsc-new") {
        mode = ARTIFACT_NTSC_NEW;
    } else {
        qDebug() << "Warning: Unknown artifact mode:" << artifactMode << "- using NONE";
        mode = ARTIFACT_NONE;
    }
    
    // Apply the artifact setting immediately
    ARTIFACT_Set(mode);
    qDebug() << "Artifact mode set to:" << mode;
}

// FUTURE: Scanlines method (commented out - not working)
// bool AtariEmulator::needsScanlineRestart() const
// {
//     // In libatari800, scanlines require restart since no SDL runtime control
//     return true;
// }

void AtariEmulator::setEmulationSpeed(int percentage)
{
    // Set the speed: 0 = unlimited turbo, percentage otherwise  
    if (percentage <= 0) {
        // Invalid input, set to 100%
        Atari800_turbo_speed = 100;
        Atari800_turbo = 0;
    } else {
        Atari800_turbo_speed = percentage;
        // Enable turbo mode if speed is not 100%
        Atari800_turbo = (percentage != 100) ? 1 : 0;
    }
    
    qDebug() << "Emulation speed set to:" << percentage << "% (turbo=" << Atari800_turbo << ")";
}

void AtariEmulator::injectCharacter(char ch)
{
    // Clear previous input
    libatari800_clear_input_array(&m_currentInput);
    // Restore joystick center positions after clearing (using libatari800-compatible center)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.joy1 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
    
    if (ch >= 'A' && ch <= 'Z') {
        // Uppercase letters - use keycode approach
        unsigned char atariKey = 0;
        switch (ch) {
            case 'A': atariKey = AKEY_a; break;
            case 'B': atariKey = AKEY_b; break;
            case 'C': atariKey = AKEY_c; break;
            case 'D': atariKey = AKEY_d; break;
            case 'E': atariKey = AKEY_e; break;
            case 'F': atariKey = AKEY_f; break;
            case 'G': atariKey = AKEY_g; break;
            case 'H': atariKey = AKEY_h; break;
            case 'I': atariKey = AKEY_i; break;
            case 'J': atariKey = AKEY_j; break;
            case 'K': atariKey = AKEY_k; break;
            case 'L': m_currentInput.keychar = 'l'; return;
            case 'M': atariKey = AKEY_m; break;
            case 'N': atariKey = AKEY_n; break;
            case 'O': atariKey = AKEY_o; break;
            case 'P': atariKey = AKEY_p; break;
            case 'Q': atariKey = AKEY_q; break;
            case 'R': atariKey = AKEY_r; break;
            case 'S': atariKey = AKEY_s; break;
            case 'T': atariKey = AKEY_t; break;
            case 'U': atariKey = AKEY_u; break;
            case 'V': atariKey = AKEY_v; break;
            case 'W': atariKey = AKEY_w; break;
            case 'X': atariKey = AKEY_x; break;
            case 'Y': atariKey = AKEY_y; break;
            case 'Z': atariKey = AKEY_z; break;
        }
        if (atariKey != 0) {
            m_currentInput.keycode = atariKey;
        }
    } else if (ch >= 'a' && ch <= 'z') {
        // Lowercase letters - use keycode approach
        unsigned char atariKey = 0;
        switch (ch) {
            case 'a': atariKey = AKEY_a; break;
            case 'b': atariKey = AKEY_b; break;
            case 'c': atariKey = AKEY_c; break;
            case 'd': atariKey = AKEY_d; break;
            case 'e': atariKey = AKEY_e; break;
            case 'f': atariKey = AKEY_f; break;
            case 'g': atariKey = AKEY_g; break;
            case 'h': atariKey = AKEY_h; break;
            case 'i': atariKey = AKEY_i; break;
            case 'j': atariKey = AKEY_j; break;
            case 'k': atariKey = AKEY_k; break;
            case 'l': m_currentInput.keychar = 'l'; return;
            case 'm': atariKey = AKEY_m; break;
            case 'n': atariKey = AKEY_n; break;
            case 'o': atariKey = AKEY_o; break;
            case 'p': atariKey = AKEY_p; break;
            case 'q': atariKey = AKEY_q; break;
            case 'r': atariKey = AKEY_r; break;
            case 's': atariKey = AKEY_s; break;
            case 't': atariKey = AKEY_t; break;
            case 'u': atariKey = AKEY_u; break;
            case 'v': atariKey = AKEY_v; break;
            case 'w': atariKey = AKEY_w; break;
            case 'x': atariKey = AKEY_x; break;
            case 'y': atariKey = AKEY_y; break;
            case 'z': atariKey = AKEY_z; break;
        }
        if (atariKey != 0) {
            m_currentInput.keycode = atariKey;
        }
    } else if (ch >= '0' && ch <= '9') {
        // Numbers - use keychar approach
        m_currentInput.keychar = ch;
    } else if (ch == ' ') {
        // Space
        m_currentInput.keychar = ' ';
    } else if (ch == '\r' || ch == '\n') {
        // Return
        m_currentInput.keycode = AKEY_RETURN;
    } else if (ch == '"') {
        // Quote - use keychar
        m_currentInput.keychar = '"';
    } else {
        return;
    }
}

void AtariEmulator::clearInput()
{
    libatari800_clear_input_array(&m_currentInput);
    // Restore joystick center positions after clearing (using libatari800-compatible center)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.joy1 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
}

void AtariEmulator::pauseEmulation()
{
    if (!m_emulationPaused) {
        m_frameTimer->stop();
        m_emulationPaused = true;
        qDebug() << "Emulation paused";
    }
}

void AtariEmulator::resumeEmulation()
{
    if (m_emulationPaused) {
        m_frameTimer->start();
        m_emulationPaused = false;
        qDebug() << "Emulation resumed";
    }
}

bool AtariEmulator::isEmulationPaused() const
{
    return m_emulationPaused;
}

void AtariEmulator::stepOneFrame()
{
    if (m_emulationPaused) {
        // Execute one frame manually when paused
        processFrame();
        qDebug() << "Stepped one frame - PC:" << QString("$%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
    } else {
        qDebug() << "Cannot step frame - emulation not paused";
    }
}

void AtariEmulator::setDiskActivityCallback(std::function<void(int, bool)> callback)
{
    // Conservative approach - only trigger activity on mount/dismount events
    // No continuous background activity
    Q_UNUSED(callback);
}

void AtariEmulator::triggerDiskActivity()
{
    // Conservative approach - no background activity
    // Only mount/dismount events trigger LEDs
}

// REMOVED: PC-based monitoring replaced with libatari800 callback
/*
void AtariEmulator::checkForDiskIO()
{
    // Back to PC monitoring since SIO variables aren't working
    // But use a much simpler drive detection approach
    
    unsigned short currentPC = CPU_regPC;
    
    // Check if we're in DOS disk routines (the ranges that were working)
    bool inDiskRoutine = (currentPC >= 0x1300 && currentPC <= 0x17FF); // DOS 2.5 ranges from your log
    
    // Debug: print every 60 frames to see if we're detecting anything
    static int debugCounter = 0;
    if (++debugCounter >= 60) {
        qDebug() << "PC monitoring: currentPC=" << QString("$%1").arg(currentPC, 4, 16, QChar('0')).toUpper() 
                 << "inDiskRoutine=" << inDiskRoutine << "mountedDrives=" << m_mountedDrives.size();
        debugCounter = 0;
    }
    
    static bool wasInDiskRoutine = false;
    static int activeDrive = -1;
    
    if (inDiskRoutine && !wasInDiskRoutine) {
        // Just entered disk routine - turn LED ON
        qDebug() << "*** DISK ROUTINE DETECTED at PC:" << QString("$%1").arg(currentPC, 4, 16, QChar('0')).toUpper() << "***";
        
        // SIMPLE approach: cycle through mounted drives based on recent activity
        static int lastUsedDrive = 1;
        int driveNumber = lastUsedDrive;
        
        // If we have multiple mounted drives, try to detect which one
        if (m_mountedDrives.size() > 1) {
            // Very simple heuristic: if we recently used D1, try D2, etc.
            QList<int> drives = m_mountedDrives.values();
            std::sort(drives.begin(), drives.end());
            
            // Find next drive after the last one used
            auto it = std::find(drives.begin(), drives.end(), lastUsedDrive);
            if (it != drives.end() && (it + 1) != drives.end()) {
                driveNumber = *(it + 1);
            } else {
                driveNumber = drives.first();
            }
        } else if (!m_mountedDrives.isEmpty()) {
            driveNumber = m_mountedDrives.values().first();
        }
        
        // Better read/write detection based on PC patterns
        // Look at the specific PC addresses from actual operations
        bool isWriting = (currentPC >= 0x1640 && currentPC <= 0x1680) ||  // Some write routines
                        (currentPC >= 0x1500 && currentPC <= 0x1550);    // More write routines
        
        activeDrive = driveNumber;
        lastUsedDrive = driveNumber;
        
        qDebug() << "=== LED SIGNAL SENT ===" 
                 << "Drive D" << driveNumber << ":" << (isWriting ? "WRITE" : "READ")
                 << "PC:" << QString("$%1").arg(currentPC, 4, 16, QChar('0')).toUpper()
                 << "Mounted drives:" << m_mountedDrives;
        
        emit diskIOStart(driveNumber, isWriting);
        
    } else if (!inDiskRoutine && wasInDiskRoutine && activeDrive != -1) {
        // Exited disk routine - turn LED OFF
        qDebug() << "=== LED OFF SIGNAL SENT === Drive D" << activeDrive << ":";
        emit diskIOEnd(activeDrive);
        activeDrive = -1;
    }
    
    wasInDiskRoutine = inDiskRoutine;
}
*/

bool AtariEmulator::handleJoystickKeyboardEmulation(QKeyEvent* event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    bool isKeyPress = (event->type() == QEvent::KeyPress);
    
    qDebug() << "=== JOYSTICK KEY EVENT ===" << "Key:" << key << "Press:" << isKeyPress << "Modifiers:" << modifiers;
    
    // libatari800 XORs joystick values with 0xff, so we need to send inverted values
    // Original INPUT_STICK_* constants XORed with 0xff:
    const int INPUT_STICK_CENTRE = 0x0f ^ 0xff;   // Center: 0x0f -> 0xf0
    const int INPUT_STICK_LEFT = 0x0b ^ 0xff;     // Left: 0x0b -> 0xf4
    const int INPUT_STICK_RIGHT = 0x07 ^ 0xff;    // Right: 0x07 -> 0xf8
    const int INPUT_STICK_FORWARD = 0x0e ^ 0xff;  // Up: 0x0e -> 0xf1
    const int INPUT_STICK_BACK = 0x0d ^ 0xff;     // Down: 0x0d -> 0xf2
    const int INPUT_STICK_UL = 0x0a ^ 0xff;       // Up+Left: 0x0a -> 0xf5
    const int INPUT_STICK_UR = 0x06 ^ 0xff;       // Up+Right: 0x06 -> 0xf9
    const int INPUT_STICK_LL = 0x09 ^ 0xff;       // Down+Left: 0x09 -> 0xf6
    const int INPUT_STICK_LR = 0x05 ^ 0xff;       // Down+Right: 0x05 -> 0xfa
    
    // Track currently pressed directional keys for each joystick
    static bool joy0_up = false, joy0_down = false, joy0_left = false, joy0_right = false;
    static bool joy1_up = false, joy1_down = false, joy1_left = false, joy1_right = false;
    static bool trig0State = false;
    static bool trig1State = false;
    static bool initialized = false;
    
    // Initialize trigger states on first call
    // libatari800 inverts trigger values too: returns input->trig0 ? 0 : 1
    // So we need to send inverted values: 0 = released, 1 = pressed
    if (!initialized) {
        m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
        m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
        m_currentInput.joy0 = INPUT_STICK_CENTRE;
        m_currentInput.joy1 = INPUT_STICK_CENTRE;
        initialized = true;
    }
    
    // Determine target joysticks based on swap setting
    int numpadTargetJoy = m_swapJoysticks ? 1 : 0;  // Default: numpad=Joy0, when swapped: numpad=Joy1
    int wasdTargetJoy = m_swapJoysticks ? 0 : 1;    // Default: WASD=Joy1, when swapped: WASD=Joy0
    
    // Helper function to calculate joystick position based on directional key states
    auto calculateJoystickValue = [&](bool up, bool down, bool left, bool right) -> int {
        if (up && left) return INPUT_STICK_UL;      // Up+Left diagonal
        if (up && right) return INPUT_STICK_UR;     // Up+Right diagonal
        if (down && left) return INPUT_STICK_LL;    // Down+Left diagonal
        if (down && right) return INPUT_STICK_LR;   // Down+Right diagonal
        if (up) return INPUT_STICK_FORWARD;         // Up only
        if (down) return INPUT_STICK_BACK;          // Down only
        if (left) return INPUT_STICK_LEFT;          // Left only
        if (right) return INPUT_STICK_RIGHT;        // Right only
        return INPUT_STICK_CENTRE;                  // Center/no movement
    };

    // Check for Numpad/Arrow keys
    if ((numpadTargetJoy == 0 && m_kbdJoy0Enabled) || (numpadTargetJoy == 1 && m_kbdJoy1Enabled)) {
        switch (key) {
            case Qt::Key_8:         // Numpad 8 - UP
            case Qt::Key_Up:        // Arrow keys
                if (modifiers & Qt::KeypadModifier || key == Qt::Key_Up) {
                    if (numpadTargetJoy == 0) {
                        joy0_up = isKeyPress;
                        m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                        qDebug() << "*** JOYSTICK 0 UP" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy0:" << m_currentInput.joy0 << "***";
                    } else {
                        joy1_up = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                        qDebug() << "*** JOYSTICK 1 UP" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy1:" << m_currentInput.joy1 << "***";
                    }
                    return true;
                }
                break;
                
            case Qt::Key_2:         // Numpad 2 - DOWN
            case Qt::Key_Down:      // Arrow keys
                if (modifiers & Qt::KeypadModifier || key == Qt::Key_Down) {
                    if (numpadTargetJoy == 0) {
                        joy0_down = isKeyPress;
                        m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                        qDebug() << "*** JOYSTICK 0 DOWN" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy0:" << m_currentInput.joy0 << "***";
                    } else {
                        joy1_down = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                        qDebug() << "*** JOYSTICK 1 DOWN" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy1:" << m_currentInput.joy1 << "***";
                    }
                    return true;
                }
                break;
                
            case Qt::Key_4:         // Numpad 4 - LEFT
            case Qt::Key_Left:      // Arrow keys
                if (modifiers & Qt::KeypadModifier || key == Qt::Key_Left) {
                    if (numpadTargetJoy == 0) {
                        joy0_left = isKeyPress;
                        m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                        qDebug() << "*** JOYSTICK 0 LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy0:" << m_currentInput.joy0 << "***";
                    } else {
                        joy1_left = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                        qDebug() << "*** JOYSTICK 1 LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy1:" << m_currentInput.joy1 << "***";
                    }
                    return true;
                }
                break;
                
            case Qt::Key_6:         // Numpad 6 - RIGHT
            case Qt::Key_Right:     // Arrow keys
                if (modifiers & Qt::KeypadModifier || key == Qt::Key_Right) {
                    if (numpadTargetJoy == 0) {
                        joy0_right = isKeyPress;
                        m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                        qDebug() << "*** JOYSTICK 0 RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy0:" << m_currentInput.joy0 << "***";
                    } else {
                        joy1_right = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                        qDebug() << "*** JOYSTICK 1 RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Numpad/Arrow) - Joy1:" << m_currentInput.joy1 << "***";
                    }
                    return true;
                }
                break;
        }
        
        // Handle trigger for numpad joystick
        if (key == Qt::Key_Enter && (modifiers & Qt::KeypadModifier)) {
            // Numpad Enter
            // libatari800 inverts trigger values: returns input->trig0 ? 0 : 1
            // So we send: 1 = pressed, 0 = released (inverted)
            if (numpadTargetJoy == 0) {
                if (isKeyPress) {
                    trig0State = true;
                    m_currentInput.trig0 = 1;  // 1 = pressed (inverted for libatari800)
                    qDebug() << "*** JOYSTICK 0 FIRE PRESSED (Numpad Enter) ***";
                } else {
                    trig0State = false;
                    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
                    qDebug() << "*** JOYSTICK 0 FIRE RELEASED (Numpad Enter) ***";
                }
            } else {
                if (isKeyPress) {
                    trig1State = true;
                    m_currentInput.trig1 = 1;  // 1 = pressed (inverted for libatari800)
                    qDebug() << "*** JOYSTICK 1 FIRE PRESSED (Numpad Enter) ***";
                } else {
                    trig1State = false;
                    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
                    qDebug() << "*** JOYSTICK 1 FIRE RELEASED (Numpad Enter) ***";
                }
            }
            return true;
        }
    }
    
    // Check for WASD keys
    if ((wasdTargetJoy == 0 && m_kbdJoy0Enabled) || (wasdTargetJoy == 1 && m_kbdJoy1Enabled)) {
        switch (key) {
            case Qt::Key_W:         // W - UP
                if (wasdTargetJoy == 0) {
                    joy0_up = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                    qDebug() << "*** JOYSTICK 0 UP" << (isKeyPress ? "PRESSED" : "RELEASED") << "(W) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    joy1_up = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                    qDebug() << "*** JOYSTICK 1 UP" << (isKeyPress ? "PRESSED" : "RELEASED") << "(W) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_S:         // S - DOWN
                if (wasdTargetJoy == 0) {
                    joy0_down = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                    qDebug() << "*** JOYSTICK 0 DOWN" << (isKeyPress ? "PRESSED" : "RELEASED") << "(S) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    joy1_down = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                    qDebug() << "*** JOYSTICK 1 DOWN" << (isKeyPress ? "PRESSED" : "RELEASED") << "(S) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_A:         // A - LEFT
                if (wasdTargetJoy == 0) {
                    joy0_left = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                    qDebug() << "*** JOYSTICK 0 LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(A) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    joy1_left = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                    qDebug() << "*** JOYSTICK 1 LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(A) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_D:         // D - RIGHT
                if (wasdTargetJoy == 0) {
                    joy0_right = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                    qDebug() << "*** JOYSTICK 0 RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(D) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    joy1_right = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                    qDebug() << "*** JOYSTICK 1 RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(D) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_Space:     // Space - FIRE for WASD joystick
                // libatari800 inverts trigger values: returns input->trig0 ? 0 : 1
                // So we send: 1 = pressed, 0 = released (inverted)
                if (wasdTargetJoy == 0) {
                    if (isKeyPress) {
                        trig0State = true;
                        m_currentInput.trig0 = 1;  // 1 = pressed (inverted for libatari800)
                        qDebug() << "*** JOYSTICK 0 FIRE PRESSED (Space) ***";
                    } else {
                        trig0State = false;
                        m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
                        qDebug() << "*** JOYSTICK 0 FIRE RELEASED (Space) ***";
                    }
                } else {
                    if (isKeyPress) {
                        trig1State = true;
                        m_currentInput.trig1 = 1;  // 1 = pressed (inverted for libatari800)
                        qDebug() << "*** JOYSTICK 1 FIRE PRESSED (Space) ***";
                    } else {
                        trig1State = false;
                        m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
                        qDebug() << "*** JOYSTICK 1 FIRE RELEASED (Space) ***";
                    }
                }
                return true;
                
            // Diagonal keys for WASD joystick - using direct diagonal constants
            case Qt::Key_Q:         // Q - UP+LEFT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UL : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 0 UP+LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Q) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UL : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 1 UP+LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Q) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_E:         // E - UP+RIGHT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UR : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 0 UP+RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(E) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UR : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 1 UP+RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(E) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_Z:         // Z - DOWN+LEFT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LL : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 0 DOWN+LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Z) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LL : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 1 DOWN+LEFT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(Z) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
                
            case Qt::Key_C:         // C - DOWN+RIGHT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LR : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 0 DOWN+RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(C) - Joy0:" << m_currentInput.joy0 << "***";
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LR : INPUT_STICK_CENTRE;
                    qDebug() << "*** JOYSTICK 1 DOWN+RIGHT" << (isKeyPress ? "PRESSED" : "RELEASED") << "(C) - Joy1:" << m_currentInput.joy1 << "***";
                }
                return true;
        }
    }
    
    return false; // Key not handled by joystick emulation
}

// SIO patch control functions for disk speed investigation
bool AtariEmulator::getSIOPatchEnabled() const
{
    // Access libatari800 SIO patch status
    extern int libatari800_get_sio_patch_enabled();
    return libatari800_get_sio_patch_enabled() != 0;
}

bool AtariEmulator::setSIOPatchEnabled(bool enabled)
{
    // Control libatari800 SIO patch status
    extern int libatari800_set_sio_patch_enabled(int enabled);
    int previousState = libatari800_set_sio_patch_enabled(enabled ? 1 : 0);
    
    qDebug() << "SIO Patch state changed from" << (previousState ? "ENABLED" : "DISABLED") 
             << "to" << (enabled ? "ENABLED" : "DISABLED");
    qDebug() << "Disk access is now" << (enabled ? "FAST (bypasses realistic timing)" : "REALISTIC (hardware timing)");
    
    return previousState != 0;
}

void AtariEmulator::debugSIOPatchStatus() const
{
    bool isEnabled = getSIOPatchEnabled();
    
    qDebug() << "=== SIO PATCH STATUS DEBUG ===";
    qDebug() << "SIO Patch (Fast Disk Access):" << (isEnabled ? "ENABLED" : "DISABLED");
    qDebug() << "Disk Access Speed:" << (isEnabled ? "FAST (emulated)" : "REALISTIC (hardware timing)");
    qDebug() << "Description:" << (isEnabled ? 
        "Disk operations bypass sector delays for faster loading" :
        "Disk operations use realistic timing delays (~3200 scanlines between sectors)");
    qDebug() << "=====================================";
    
    // Also check compile-time settings
    #ifdef NO_SECTOR_DELAY
    qDebug() << "COMPILE FLAG: NO_SECTOR_DELAY is DEFINED (delays disabled at compile time)";
    #else
    qDebug() << "COMPILE FLAG: NO_SECTOR_DELAY is NOT DEFINED (delays available if SIO patch disabled)";
    #endif
}