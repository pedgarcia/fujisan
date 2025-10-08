/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifdef _WIN32
#include "windows_compat.h"
#endif

#include "atariemulator.h"
#include <QDebug>
#include <QApplication>
#include <QMetaObject>
#include <QTimer>
#include <QThread>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>
#include <cstring>  // for memset
#include <vector>   // for std::vector

#ifdef HAVE_SDL2_AUDIO
#include "sdl2audiobackend.h"
#include "unifiedaudiobackend.h"
#endif

#ifdef HAVE_SDL2_JOYSTICK
#include "sdl2joystickmanager.h"
#endif

extern "C" {
#ifdef NETSIO
#include "../src/netsio.h"
#endif
#include "../src/rtime.h"
#include "../src/binload.h"
// Printer support functions
void ESC_PatchOS(void);
void Devices_UpdatePatches(void);

// Access to CPU registers for XEX loading
extern unsigned char CPU_regS;
extern unsigned short CPU_regPC;
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
    , m_audioBackend(QtAudio)  // Revert to Qt audio as default (unified backend needs debugging)
    , m_unifiedAudio(nullptr)
#ifdef HAVE_SDL2_AUDIO
    , m_sdl2Audio(nullptr)
#endif
    , m_audioOutput(nullptr)
    , m_audioDevice(nullptr)
    , m_audioEnabled(true)
    , m_dspBufferBytes(0)
    , m_dspWritePos(0)
    , m_dspReadPos(0)
    , m_callbackTick(0)
    , m_avgGap(0.0)
    , m_targetDelay(0)
    , m_currentSpeed(1.0)
    , m_targetSpeed(1.0)
    , m_sampleRate(44100)
    , m_bytesPerSample(4)
    , m_fragmentSize(1024)
    , m_userRequestedSpeedMultiplier(1.0)
    , m_printerEnabled(false)
    , m_fujinet_restart_pending(false)
    , m_fujinet_restart_delay(0)
    , m_usePartialFrameExecution(false)
    , m_cyclesThisFrame(0)
#ifdef HAVE_SDL2_JOYSTICK
    , m_joystickManager(nullptr)
    , m_realJoysticksEnabled(false)
    , m_joystick1AssignedDevice("keyboard")
    , m_joystick2AssignedDevice("keyboard")
#endif
{
    libatari800_clear_input_array(&m_currentInput);

    // Initialize joysticks to center position (inverted for libatari800)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // 15 ^ 255 = 240
    m_currentInput.joy1 = 0x0f ^ 0xff;  // 15 ^ 255 = 240
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)

    // Set up the global instance pointer for the callback
    s_emulatorInstance = this;
    
    // Use PreciseTimer for more accurate frame timing
    // This might help with audio synchronization issues
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    connect(m_frameTimer, &QTimer::timeout, this, &AtariEmulator::processFrame);

#ifdef HAVE_SDL2_JOYSTICK
    // Initialize SDL2 joystick manager
    qDebug() << "Initializing SDL2 joystick support...";
    m_joystickManager = new SDL2JoystickManager(this);
    if (!m_joystickManager->initialize()) {
        qWarning() << "Failed to initialize SDL2 joystick subsystem - joystick support disabled";
        qWarning() << "On Linux, ensure your user is in the 'input' group: sudo usermod -a -G input $USER";
        delete m_joystickManager;
        m_joystickManager = nullptr;
    } else {
        qDebug() << "SDL2 joystick manager initialized successfully";
    }
#else
    qDebug() << "SDL2 joystick support not compiled in - joystick support unavailable";
#endif
}

AtariEmulator::~AtariEmulator()
{
    // Clear the global instance pointer
    if (s_emulatorInstance == this) {
        s_emulatorInstance = nullptr;
        libatari800_set_disk_activity_callback(nullptr);
    }
    shutdown();

    // Clean up unified audio backend
#ifdef HAVE_SDL2_AUDIO
    if (m_unifiedAudio) {
        delete m_unifiedAudio;
        m_unifiedAudio = nullptr;
    }
#endif

#ifdef HAVE_SDL2_AUDIO
    if (m_sdl2Audio) {
        delete m_sdl2Audio;
        m_sdl2Audio = nullptr;
    }
#endif
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
    if (artifactMode != "none") {
        if (videoSystem == "-ntsc") {
            // Only ntsc-old and ntsc-new are supported (ntsc-full disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-ntsc-artif" << artifactMode;
            }
        } else if (videoSystem == "-pal") {
            // Map NTSC artifact modes to PAL simple (pal-blend disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-pal-artif" << "pal-simple";
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
    
    // TODO: Display parameters - commented out because libatari800 doesn't support them
    // These would work with full SDL atari800 build:
    // argList << "-horiz-area" << horizontalArea;
    // argList << "-vert-area" << verticalArea;
    // if (horizontalShift != 0) argList << "-horiz-shift" << QString::number(horizontalShift);
    // if (verticalShift != 0) argList << "-vert-shift" << QString::number(verticalShift);
    // argList << "-fit-screen" << fitScreen;
    // if (show80Column) argList << "-80column"; else argList << "-no-80column";
    // if (vSyncEnabled) argList << "-vsync"; else argList << "-no-vsync";
    
    
    // Add audio configuration
    if (m_audioEnabled) {
        argList << "-sound";
        
        // Get audio frequency from settings (use same organization as settings dialog)
        QSettings settings("8bitrelics", "Fujisan");
        int audioFreq = settings.value("audio/frequency", 44100).toInt();
        argList << "-dsprate" << QString::number(audioFreq);
        
        // At 22050Hz, we generate half the data:
        // 22050 * 2 bytes / 59.92fps = 736 bytes/frame (instead of 1472)
        
        argList << "-audio16";
        argList << "-volume" << "80";
        // Don't use -sound-quality as it might not be recognized
        // NOTE: -speaker argument causes "Error opening" in libatari800 minimal build
        // Console speaker functionality handled internally
    } else {
        argList << "-nosound";
    }
    
    
    // Configure OS ROM (Altirra or External)
    if (m_altirraOSEnabled) {
        if (machineType == "-5200") {
            argList << "-5200-rev" << "altirra";
        } else if (machineType == "-atari") {
            argList << "-800-rev" << "altirra";
        } else {
            argList << "-xl-rev" << "altirra";
        }
    } else {
        // Only add ROM paths if they are specified in settings and file exists
        if (!m_osRomPath.isEmpty()) {
            QFileInfo osRomFile(m_osRomPath);
            if (osRomFile.exists()) {
                // NOTE: Do NOT use quotePath() here! We're passing arguments via char* array (argc/argv),
                // not as a shell command string. Quotes are only needed for shell parsing.
                // Each QStringList element becomes a separate argv[] entry, so spaces are handled correctly.
                if (machineType == "-5200") {
                    argList << "-5200_rom" << m_osRomPath;
                } else if (machineType == "-atari") {
                    argList << "-osb_rom" << m_osRomPath;  // 800 OS-B ROM
                } else {
                    // For XL/XE machines
                    argList << "-xlxe_rom" << m_osRomPath;
                }
            } else {
                // File doesn't exist - fallback to Altirra OS
                qWarning() << "OS ROM file not found at" << m_osRomPath << "- Loading Altirra OS";
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "altirra";
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "altirra";
                } else {
                    argList << "-xl-rev" << "altirra";
                }
            }
        } else {
            // Fallback to Altirra OS if no external ROM is specified
            if (machineType == "-5200") {
                argList << "-5200-rev" << "altirra";
            } else if (machineType == "-atari") {
                argList << "-800-rev" << "altirra";
            } else {
                argList << "-xl-rev" << "altirra";
            }
        }
    }

    // Configure BASIC ROM (Altirra, External, or None)
    if (basicEnabled) {
        if (m_altirraBASICEnabled) {
            argList << "-basic-rev" << "altirra";
        } else {
            if (!m_basicRomPath.isEmpty()) {
                QFileInfo basicRomFile(m_basicRomPath);
                if (basicRomFile.exists()) {
                    // NOTE: Do NOT quote - we're using char* array, not shell string
                    argList << "-basic_rom" << m_basicRomPath;
                } else {
                    // File doesn't exist - fallback to Altirra BASIC
                    qWarning() << "BASIC ROM file not found at" << m_basicRomPath << "- Loading Altirra BASIC";
                    argList << "-basic-rev" << "altirra";
                }
            } else {
                argList << "-basic-rev" << "altirra";
            }
        }
        argList << "-basic";  // Actually enable BASIC mode
    } else {
        argList << "-nobasic";
    }

    // Convert QStringList to char* array
    QList<QByteArray> argBytes;
    for (const QString& arg : argList) {
        argBytes.append(arg.toUtf8());
    }
    
    std::vector<char*> args(argBytes.size() + 1);
    for (int i = 0; i < argBytes.size(); ++i) {
        args[i] = argBytes[i].data();
    }
    args[argBytes.size()] = nullptr;
    
    
    // Force complete libatari800 reset to clear any persistent ROM state
    // This ensures ROM configuration changes are applied properly
    static bool libatari800_previously_initialized = false;
    if (libatari800_previously_initialized) {
        libatari800_exit();
    } else {
    }
    
    if (libatari800_init(argBytes.size(), args.data())) {
        libatari800_previously_initialized = true;  // Mark as initialized for future resets
        
        // Verify ROM loading status
        if (!m_altirraOSEnabled && !m_osRomPath.isEmpty()) {
        } else if (m_altirraOSEnabled) {
        } else {
        }
        
        if (basicEnabled && !m_altirraBASICEnabled && !m_basicRomPath.isEmpty()) {
        } else if (basicEnabled && m_altirraBASICEnabled) {
        }
        m_targetFps = libatari800_get_fps();
        m_frameTimeMs = 1000.0f / m_targetFps;

        // Apply cartridge auto-reboot setting from QSettings
        QSettings settings("8bitrelics", "Fujisan");
        CARTRIDGE_autoreboot = settings.value("machine/cartridgeAutoReboot", true).toBool() ? 1 : 0;
        qDebug() << "CARTRIDGE_autoreboot initialized to:" << CARTRIDGE_autoreboot;

        // Set up the disk activity callback for hardware-level monitoring
        libatari800_set_disk_activity_callback(diskActivityCallback);
        
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
    
    return false;
}

bool AtariEmulator::initializeWithInputConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                            const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                            const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                            bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks, bool netSIOEnabled)
{
    
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
    
    // Configure keyboard joystick emulation based on settings
    // NOTE: libatari800 minimal build doesn't support -kbdjoy0/-kbdjoy1 arguments
    // These cause "Error opening" messages and break command line parsing
    // Keyboard joystick functionality is handled via direct key injection instead
    if (kbdJoy0Enabled) {
    } else {
    }
    
    if (kbdJoy1Enabled) {
    } else {
    }
    
    // Add artifact settings - only supported modes for current build
    if (artifactMode != "none") {
        if (videoSystem == "-ntsc") {
            // Only ntsc-old and ntsc-new are supported (ntsc-full disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-ntsc-artif" << artifactMode;
            }
        } else if (videoSystem == "-pal") {
            // Map NTSC artifact modes to PAL simple (pal-blend disabled in build)
            if (artifactMode == "ntsc-old" || artifactMode == "ntsc-new") {
                argList << "-pal-artif" << "pal-simple";
            }
        }
    }
    
    
    // Audio configuration
    if (m_audioEnabled) {
        argList << "-sound";
        
        // Get audio frequency from settings (use same organization as settings dialog)
        QSettings settings("8bitrelics", "Fujisan");
        int audioFreq = settings.value("audio/frequency", 44100).toInt();
        argList << "-dsprate" << QString::number(audioFreq);
        
        argList << "-audio16";
        argList << "-volume" << "80";
        // NOTE: -speaker argument causes "Error opening" in libatari800 minimal build
        // Console speaker functionality handled internally
    } else {
        argList << "-nosound";
    }
    
    // Configure OS ROM (Altirra or External)
    if (m_altirraOSEnabled) {
        // Use built-in Altirra OS for all machine types
        argList << "-xl-rev" << "altirra";
    } else {
        if (!m_osRomPath.isEmpty()) {
            QFileInfo osRomFile(m_osRomPath);
            if (osRomFile.exists()) {
                // NOTE: Do NOT use quotePath() - we're using char* array, not shell string
                if (machineType == "-5200") {
                    argList << "-5200_rom" << m_osRomPath;
                } else if (machineType == "-atari") {
                    argList << "-osb_rom" << m_osRomPath;  // 800 OS-B ROM
                } else {
                    // For XL/XE machines
                    argList << "-xlxe_rom" << m_osRomPath;
                }
            } else {
                // File doesn't exist - fallback to Altirra OS
                qWarning() << "OS ROM file not found at" << m_osRomPath << "- Loading Altirra OS";
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "altirra";
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "altirra";
                } else {
                    argList << "-xl-rev" << "altirra";
                }
            }
        } else {
            // Fallback to Altirra OS if no external ROM is specified
            if (machineType == "-5200") {
                argList << "-5200-rev" << "altirra";
            } else if (machineType == "-atari") {
                argList << "-800-rev" << "altirra";
            } else {
                argList << "-xl-rev" << "altirra";
            }
        }
    }
    
    // Configure BASIC ROM (Altirra, External, or None)
    if (basicEnabled) {
        if (m_altirraBASICEnabled) {
            argList << "-basic-rev" << "altirra";
        } else {
            if (!m_basicRomPath.isEmpty()) {
                QFileInfo basicRomFile(m_basicRomPath);
                if (basicRomFile.exists()) {
                    // NOTE: Do NOT quote - we're using char* array, not shell string
                    argList << "-basic_rom" << m_basicRomPath;
                } else {
                    qWarning() << "BASIC ROM file not found at" << m_basicRomPath << "- Loading Altirra BASIC";
                    argList << "-basic-rev" << "altirra";
                }
            } else {
                argList << "-basic-rev" << "altirra";
            }
        }
        argList << "-basic";  // Actually enable BASIC mode
    } else {
        argList << "-nobasic";
    }
    
    // Add NetSIO support if enabled
    if (netSIOEnabled) {
        argList << "-netsio";
    }
    
    // Initialize printer support - DISABLED (P: device not working in atari800 core)
    // TODO: Re-enable when P: device emulation is fixed
    /*
    QSettings settings;
    bool printerEnabled = settings.value("printer/enabled", false).toBool();
    if (printerEnabled) {
        // Enable P: device before atari800 core initialization
        extern int Devices_enable_p_patch;
        Devices_enable_p_patch = 1;
    }
    */
    bool printerEnabled = false; // Force disabled
    
    // Add device debugging to help troubleshoot P: device issues
    argList << "-devbug";
    
    // Convert QStringList to char* array
    QList<QByteArray> argBytes;
    for (const QString& arg : argList) {
        argBytes.append(arg.toUtf8());
    }
    
    std::vector<char*> args(argBytes.size() + 1);
    for (int i = 0; i < argBytes.size(); ++i) {
        args[i] = argBytes[i].data();
    }
    args[argBytes.size()] = nullptr;
    
    
    // Force complete libatari800 reset to clear any persistent ROM state
    // This ensures ROM configuration changes are applied properly
    static bool libatari800_previously_initialized = false;
    if (libatari800_previously_initialized) {
        libatari800_exit();
    } else {
    }
    
    if (libatari800_init(argBytes.size(), args.data())) {
        libatari800_previously_initialized = true;  // Mark as initialized for future resets
        
        // Verify ROM loading status
        if (!m_altirraOSEnabled && !m_osRomPath.isEmpty()) {
        } else if (m_altirraOSEnabled) {
        } else {
        }
        
        if (basicEnabled && !m_altirraBASICEnabled && !m_basicRomPath.isEmpty()) {
        } else if (basicEnabled && m_altirraBASICEnabled) {
        }
        
#ifdef GUI_SDL
        // Check actual keyboard joystick state after initialization
        extern int PLATFORM_IsKbdJoystickEnabled(int num);
        bool actualKbdJoy0 = PLATFORM_IsKbdJoystickEnabled(0);
        bool actualKbdJoy1 = PLATFORM_IsKbdJoystickEnabled(1);
        
        if (actualKbdJoy0 != kbdJoy0Enabled || actualKbdJoy1 != kbdJoy1Enabled) {
        }
#endif
        
        m_targetFps = libatari800_get_fps();
        m_frameTimeMs = 1000.0f / m_targetFps;

        // Apply cartridge auto-reboot setting from QSettings
        QSettings settings("8bitrelics", "Fujisan");
        CARTRIDGE_autoreboot = settings.value("machine/cartridgeAutoReboot", true).toBool() ? 1 : 0;
        qDebug() << "CARTRIDGE_autoreboot initialized to:" << CARTRIDGE_autoreboot;

        // Set up the disk activity callback for hardware-level monitoring
        libatari800_set_disk_activity_callback(diskActivityCallback);
        
        // Set up printer if enabled - DISABLED
        /*
        if (printerEnabled) {
            // Use a command that copies the spool file to a visible location
            // This ensures we can see the printer output and avoid timeout
            QString fujisanPrintCommand = QString("cp %s /tmp/atari_printer_output.txt && echo 'Printer output saved to /tmp/atari_printer_output.txt'");
            Devices_SetPrintCommand(fujisanPrintCommand.toUtf8().constData());
            
            // Explicitly patch the OS to install P: device handlers
            ESC_PatchOS();
        }
        */
        
        // Initialize audio output if enabled
        if (m_audioEnabled) {
            setupAudio();
        }
        
        // Start the frame timer
        m_frameTimer->start(static_cast<int>(m_frameTimeMs));
        return true;
    }
    
    return false;
}

bool AtariEmulator::initializeWithNetSIOConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                             const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                             const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                             bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks,
                                             bool netSIOEnabled, bool rtimeEnabled)
{
    
    // CRITICAL: NetSIO/FujiNet requires BASIC to be disabled for proper booting
    bool actualBasicEnabled = basicEnabled;
    if (netSIOEnabled && basicEnabled) {
        actualBasicEnabled = false;
        // Update the emulator's internal BASIC state to reflect the auto-disable
        setBasicEnabled(false);
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
        
        // CRITICAL: Dismount all local disks to give FujiNet boot priority
        // When NetSIO is enabled, FujiNet devices should take precedence over local ATR files
        for (int i = 1; i <= 8; i++) {
            dismountDiskImage(i);
        }
        
        // Use delayed restart mechanism like Atari800MacX for proper FujiNet timing
        m_fujinet_restart_pending = true;
        m_fujinet_restart_delay = 60; // Wait 60 frames (~1 second) 
        
        // Send test command to verify NetSIO communication with FujiNet-PC
#ifdef NETSIO
        extern void netsio_test_cmd(void);
        netsio_test_cmd();
#endif
    }
    
    // Enable R-Time 8 if requested
    if (rtimeEnabled) {
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
        }
        if (m_fujinet_restart_delay == 0) {
            Atari800_InitialiseMachine();  // Function already declared in atari.h
            m_fujinet_restart_pending = false;
        }
    }

    // Process device-specific joystick input
#ifdef HAVE_SDL2_JOYSTICK
    if (m_joystickManager) {
        // Process Joystick 1 based on assigned device
        if (m_joystick1AssignedDevice.startsWith("sdl_")) {
            // Extract SDL joystick index from device string (e.g., "sdl_0" -> 0)
            bool ok;
            int sdlIndex = m_joystick1AssignedDevice.mid(4).toInt(&ok);
            if (ok) {
                JoystickState joyState = m_joystickManager->getJoystickState(sdlIndex);
                if (joyState.connected) {
                    int targetJoy = m_swapJoysticks ? 1 : 0;  // Apply swapping
                    if (targetJoy == 0) {
                        m_currentInput.joy0 = joyState.stick;
                        m_currentInput.trig0 = joyState.trigger ? 1 : 0;
                    } else {
                        m_currentInput.joy1 = joyState.stick;
                        m_currentInput.trig1 = joyState.trigger ? 1 : 0;
                    }
                }
            }
        }
        // Note: "keyboard" and "none" devices are handled by keyboard emulation/no input

        // Process Joystick 2 based on assigned device
        if (m_joystick2AssignedDevice.startsWith("sdl_")) {
            // Extract SDL joystick index from device string (e.g., "sdl_1" -> 1)
            bool ok;
            int sdlIndex = m_joystick2AssignedDevice.mid(4).toInt(&ok);
            if (ok) {
                JoystickState joyState = m_joystickManager->getJoystickState(sdlIndex);
                if (joyState.connected) {
                    int targetJoy = m_swapJoysticks ? 0 : 1;  // Apply swapping
                    if (targetJoy == 0) {
                        m_currentInput.joy0 = joyState.stick;
                        m_currentInput.trig0 = joyState.trigger ? 1 : 0;
                    } else {
                        m_currentInput.joy1 = joyState.stick;
                        m_currentInput.trig1 = joyState.trigger ? 1 : 0;
                    }
                }
            }
        }
        // Note: "keyboard" and "none" devices are handled by keyboard emulation/no input
    }
#endif

    // Send joystick values to libatari800

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
    
    // Determine if we should use partial frame execution for precise breakpoints
    m_usePartialFrameExecution = m_breakpointsEnabled && !m_breakpoints.isEmpty() && !m_emulationPaused;
    
    // For now, disable partial frame execution since it requires patches
    // We'll use frame-level execution with breakpoint checking after each frame
    // This is less precise but works with standard libatari800
    if (false && m_usePartialFrameExecution) {
        // Partial frame execution disabled - requires patches
    } else {
        // Normal full-frame execution
        libatari800_next_frame(&m_currentInput);

        // Check breakpoints once after the frame (legacy behavior)
        if (m_breakpointsEnabled && !m_breakpoints.isEmpty()) {
            checkBreakpoints();
        }
    }
    
    // Disk I/O monitoring is now handled by libatari800 callback

#ifdef HAVE_SDL2_AUDIO
    // Handle audio output based on backend type
    if (m_audioBackend == UnifiedAudio && m_unifiedAudio && m_audioEnabled) {
        // Unified Audio Backend - submit audio with priority classification
        unsigned char* soundBuffer = libatari800_get_sound_buffer();
        int soundBufferLen = libatari800_get_sound_buffer_len();

        if (soundBuffer && soundBufferLen > 0) {
            // For now, treat all audio as low priority (background music/game sounds)
            // High priority would be for UI beeps, key clicks, etc.
            // TODO: Implement audio classification based on sound source
            UnifiedAudioBackend::AudioPriority priority = UnifiedAudioBackend::LOW_PRIORITY;

            bool success = m_unifiedAudio->submitAudio(soundBuffer, soundBufferLen, priority);
            if (!success) {
                // Buffer overrun - this should be rare with the new architecture
                static int overrunCount = 0;
                if (++overrunCount % 100 == 1) {
                    qDebug() << "Audio buffer overrun #" << overrunCount
                             << "- buffer fill:" << m_unifiedAudio->getBufferFillPercent() << "%";
                }
            }

            // Periodically check performance and adjust latency if needed
            static int frameCount = 0;
            if (++frameCount % 300 == 0) {  // Check every 300 frames (~5 seconds)
                m_unifiedAudio->checkPerformanceAndAdjust();
                m_unifiedAudio->adjustLatencyForPerformance();
            }
        }
    }
#endif
#ifdef HAVE_SDL2_AUDIO
    // Legacy SDL2 audio backend
    if (m_audioBackend == SDL2Audio && m_sdl2Audio && m_sdl2Audio->isInitialized()) {
        unsigned char* soundBuffer = libatari800_get_sound_buffer();
        int soundBufferLen = libatari800_get_sound_buffer_len();
        
        if (soundBuffer && soundBufferLen > 0) {
            QMutexLocker locker(&m_sdl2AudioMutex);
            
            // Calculate current buffer level
            int currentLevel = (m_sdl2WritePos - m_sdl2ReadPos + SDL2_BUFFER_SIZE) % SDL2_BUFFER_SIZE;
            int availableSpace = SDL2_BUFFER_SIZE - currentLevel - 1;
            
            // Simple buffer management - SDL2 buffer now matches frame size
            // At 22050Hz: we produce 735 bytes, SDL2 consumes 736 bytes
            // At 44100Hz: we produce 1470 bytes, SDL2 consumes 1472 bytes
            // This should maintain balance without skipping
            
            if (availableSpace >= soundBufferLen) {
                // Write the audio data
                for (int i = 0; i < soundBufferLen; i++) {
                    m_sdl2AudioBuffer[m_sdl2WritePos] = soundBuffer[i];
                    m_sdl2WritePos = (m_sdl2WritePos + 1) % SDL2_BUFFER_SIZE;
                }
            } else {
                // Buffer full - this should be rare with matched sizes
                static int skipCount = 0;
                skipCount++;
                if (skipCount <= 10 || skipCount % 100 == 0) {
                }
            }
        }
    } else
#endif
    // Handle Qt audio output with double buffering (inspired by Atari800MacX)
    if (m_audioEnabled && m_audioOutput && m_audioDevice) {
        unsigned char* soundBuffer = libatari800_get_sound_buffer();
        int soundBufferLen = libatari800_get_sound_buffer_len();
        
        // Safety check for buffer initialization
        if (m_dspBufferBytes == 0 || m_dspBuffer.isEmpty()) {
            return;  // Audio not properly initialized yet
        }
        
        if (soundBuffer && soundBufferLen > 0) {
            // Write to DSP buffer (producer side)
            int gap = m_dspWritePos - m_dspReadPos;
            
            // Handle wrap-around
            if (gap < 0) {
                gap += m_dspBufferBytes;
            }
            
            // Conditional speed adjustment - disabled for Windows due to Qt audio issues
#ifndef _WIN32
            updateEmulationSpeed();
#endif

            // Platform-specific buffer management
            int targetGap = m_targetDelay * m_bytesPerSample;  // Target buffer level

#ifdef _WIN32
            // Windows: Fix underrun issue - buffer is too empty, not too full!
            // The drops are Qt audio underruns, not our frame drops

            static int frameCount = 0;
            frameCount++;

            // Log underrun conditions (disabled - audio now stable)
            // Underruns during window operations are expected/acceptable
            /*
            if (gap < targetGap * 0.5) {  // Buffer less than 50% of target
                static int underrunCount = 0;
                if (++underrunCount % 100 == 1) {
                                .arg(underrunCount)
                                .arg(frameCount)
                                .arg(gap)
                                .arg(targetGap > 0 ? (gap * 100 / targetGap) : 0)
                                .arg(targetGap);
                }
            }
            */

            // Windows audio debug (disabled - audio now stable)
            // Uncomment for troubleshooting if needed
            /*
            static int debugCount = 0;
            if (++debugCount % 100 == 1) {
                int available = m_dspWritePos - m_dspReadPos;
                if (available < 0) available += m_dspBufferBytes;
                int bytesFree = m_audioOutput ? m_audioOutput->bytesFree() : -1;
                            .arg(debugCount).arg(frameCount);
                            .arg(m_dspWritePos).arg(m_dspReadPos).arg(available);
                            .arg(soundBufferLen).arg(bytesFree).arg(gap).arg(targetGap);
            }
            */

            // Always write audio data - we need MORE data, not less
            // Never drop frames - the problem is underruns, not overruns
#else
            // macOS/Linux: Use adaptive system (keep original behavior)
            if (gap > targetGap * 3) {
                static int warnCount = 0;
                if (++warnCount % 200 == 1) {
                }
            }

            // Always write audio data - use ring buffer overwrite if needed
#endif
            {
                // Write to DSP buffer
                int newPos = m_dspWritePos + soundBufferLen;
                if (newPos / m_dspBufferBytes == m_dspWritePos / m_dspBufferBytes) {
                    // No wrap
                    memcpy(m_dspBuffer.data() + (m_dspWritePos % m_dspBufferBytes), 
                           soundBuffer, soundBufferLen);
                } else {
                    // Wraps around
                    int firstPartSize = m_dspBufferBytes - (m_dspWritePos % m_dspBufferBytes);
                    memcpy(m_dspBuffer.data() + (m_dspWritePos % m_dspBufferBytes), 
                           soundBuffer, firstPartSize);
                    memcpy(m_dspBuffer.data(), 
                           soundBuffer + firstPartSize, soundBufferLen - firstPartSize);
                }
                m_dspWritePos = newPos % (m_dspBufferBytes * 2);
            }
            
            // Keep positions normalized
            while (m_dspWritePos >= m_dspBufferBytes && m_dspReadPos >= m_dspBufferBytes) {
                m_dspWritePos -= m_dspBufferBytes;
                m_dspReadPos -= m_dspBufferBytes;
            }
            
            // Write from DSP buffer to audio device (consumer side)
            int available = m_dspWritePos - m_dspReadPos;
            if (available < 0) {
                available += m_dspBufferBytes;
            }
            
            int bytesFree = m_audioOutput->bytesFree();

            // Platform-specific writing strategy
#ifdef _WIN32
            // Windows: Write in period-size chunks to match Qt's preferred rhythm
            int periodSize = m_audioOutput->periodSize();
            int toWrite = qMin(available, bytesFree);

            // Only write if we have at least a period-size worth of data
            if (toWrite >= periodSize && available >= periodSize) {
                // Round down to period-size multiple for optimal Qt performance
                toWrite = (toWrite / periodSize) * periodSize;
#else
            // macOS/Linux: Aggressive writing for low latency
            int toWrite = qMin(available, bytesFree);

            // Always try to write if there's any space available
            if (toWrite > 0 && available > 0) {
#endif
                // Read from DSP buffer and write to audio device
                int newReadPos = m_dspReadPos + toWrite;
                if (newReadPos / m_dspBufferBytes == m_dspReadPos / m_dspBufferBytes) {
                    // No wrap
                    m_audioDevice->write(
                        m_dspBuffer.data() + (m_dspReadPos % m_dspBufferBytes), 
                        toWrite
                    );
                } else {
                    // Wraps around
                    int firstPartSize = m_dspBufferBytes - (m_dspReadPos % m_dspBufferBytes);
                    m_audioDevice->write(
                        m_dspBuffer.data() + (m_dspReadPos % m_dspBufferBytes), 
                        firstPartSize
                    );
                    m_audioDevice->write(
                        m_dspBuffer.data(), 
                        toWrite - firstPartSize
                    );
                }
                m_dspReadPos = newReadPos % (m_dspBufferBytes * 2);
            }
            
            // Log double buffer stats periodically (commented out for production)
            // static int frameCount = 0;
            // if (++frameCount % 100 == 0) {
            //     int gap = m_dspWritePos - m_dspReadPos;
            //     if (gap < 0) gap += m_dspBufferBytes;
            //     
            //     int percentFull = (m_dspBufferBytes > 0) ? (gap * 100 / m_dspBufferBytes) : 0;
            //     qDebug() << "DSP Buffer - Gap:" << gap << "bytes"
            //              << "(" << percentFull << "%)"
            //              << "| Available:" << available
            //              << "| Written:" << toWrite
            //              << "| Speed:" << QString::number(m_currentSpeed, 'f', 3)
            //              << "| AvgGap:" << QString::number(m_avgGap, 'f', 1)
            //              << "| Target delay:" << (m_targetDelay * m_bytesPerSample) << "bytes";
            // }
        }
    }
    
    // Don't clear input here - let it persist until key release

    // Check breakpoints after frame execution
    checkBreakpoints();

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
        
        int result = BINLOAD_Loader(filename.toUtf8().constData());
        
        if (result) {
            return true;
        } else {
            return false;
        }
    } else {
        // Load other files (CAR, ROM, etc.) as cartridges

        qDebug() << "Loading cartridge file:" << filename;
        qDebug() << "Current CARTRIDGE_autoreboot value:" << CARTRIDGE_autoreboot;
        qDebug() << "Current Atari800_disable_basic value:" << Atari800_disable_basic;

        // CRITICAL: Disable BASIC to allow cartridge to boot
        // When Atari800_disable_basic = TRUE, it simulates holding the Option key
        // during boot, which disables BASIC and allows the cartridge to run
        Atari800_disable_basic = TRUE;
        qDebug() << "Set Atari800_disable_basic = TRUE to allow cartridge boot";

        // First, explicitly remove any existing cartridge with reboot
        // This ensures a clean slate before loading the new cartridge
        CARTRIDGE_RemoveAutoReboot();
        qDebug() << "Removed existing cartridge";

        // Now insert the new cartridge with auto-reboot
        // This provides a complete system reset with the new cartridge
        int result = CARTRIDGE_InsertAutoReboot(filename.toUtf8().constData());

        qDebug() << "CARTRIDGE_InsertAutoReboot returned:" << result;

        if (result == 0) {
            // Type was auto-detected, cartridge is ready
            qDebug() << "Cartridge type auto-detected and inserted successfully";
            return true;
        } else if (result > 0) {
            // Positive value = size in KB, type is UNKNOWN, must set type manually
            qDebug() << "Cartridge size is" << result << "KB, type is UNKNOWN, setting to CARTRIDGE_STD_8";

            // For 8KB cartridges, default to standard 8KB type
            // CARTRIDGE_SetTypeAutoReboot will initialize and map the cartridge, then reboot
            if (result == 8) {
                CARTRIDGE_SetTypeAutoReboot(&CARTRIDGE_main, CARTRIDGE_STD_8);
                qDebug() << "Cartridge type set to CARTRIDGE_STD_8 and system rebooted";
                return true;
            } else {
                qDebug() << "Unsupported cartridge size:" << result << "KB - only 8KB standard cartridges are currently supported";
                return false;
            }
        } else {
            qDebug() << "CARTRIDGE_InsertAutoReboot failed with error:" << result;
            // Fall back to the original method for non-cartridge files
            bool fallback_result = libatari800_reboot_with_file(filename.toUtf8().constData());
            qDebug() << "Fallback method returned:" << fallback_result;
            return fallback_result;
        }
    }
}

void AtariEmulator::ejectCartridge()
{
    // Remove cartridge from libatari800 and reboot
    CARTRIDGE_RemoveAutoReboot();

    // Re-enable BASIC (we disabled it when loading the cartridge)
    Atari800_disable_basic = FALSE;

    qDebug() << "Cartridge ejected from emulator";
}

bool AtariEmulator::mountDiskImage(int driveNumber, const QString& filename, bool readOnly)
{
    // Validate drive number (1-8 for D1: through D8:)
    if (driveNumber < 1 || driveNumber > 8) {
        return false;
    }
    
    if (filename.isEmpty()) {
        return false;
    }
    
    
    // Mount the disk image using libatari800
    int result = libatari800_mount_disk_image(driveNumber, filename.toUtf8().constData(), readOnly ? 1 : 0);
    
    
    if (result) {
        // Store the path for tracking
        m_diskImages[driveNumber - 1] = filename;
        m_mountedDrives.insert(driveNumber);

        qDebug() << "Disk mounted on D" << driveNumber << ":" << filename
                 << (readOnly ? "(read-only)" : "(read-write)");

        return true;
    } else {
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
    libatari800_unmount_disk(driveNumber);
    
    // Clear Fujisan internal state
    m_diskImages[driveNumber - 1].clear();
    m_mountedDrives.remove(driveNumber);
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
}

void AtariEmulator::coldRestart()
{
    // Trigger Atari800 cold start to refresh boot sequence
    Atari800_Coldstart();
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
    
    // Process key input
    
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
                qDebug() << "Key press with shift+ctrl:"
                         << "base:" << (int)baseKey << "shiftctrl:" << (int)shiftctrl << "(pure control)";
            } else {
                qDebug() << "Key press:"
                         << "base:" << (int)baseKey << "shiftctrl:" << (int)shiftctrl << "(display control)";
            }
        } else {
        }
    } else if (key == Qt::Key_CapsLock) {
        // Send CAPS LOCK toggle to the emulator to change its internal state
        m_currentInput.keycode = AKEY_CAPSTOGGLE;
    } else if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        if (key == Qt::Key_L) {
            // Special handling for L key since AKEY_l = 0 causes issues
            // Use keychar approach for L key only - send lowercase so emulator can apply case logic
            m_currentInput.keychar = 'l';  // Send lowercase, emulator handles case via CAPS LOCK
        } else {
            // Normal letter handling using keycode
            unsigned char baseKey = convertQtKeyToAtari(key, Qt::NoModifier);
            m_currentInput.keycode = baseKey;
            // Letter key
        }
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        if (shiftPressed) {
            // Handle shifted number keys
            QString shiftedSymbols = ")!@#$%^&*(";
            int index = key - Qt::Key_0;
            if (index < shiftedSymbols.length()) {
                m_currentInput.keychar = shiftedSymbols[index].toLatin1();
                // Shifted number
            }
        } else {
            m_currentInput.keychar = key - Qt::Key_0 + '0';
            // Numeric character
        }
    } else if (key == Qt::Key_Space) {
        m_currentInput.keychar = ' ';
        // Space character
    } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        // Use keycode approach for RETURN key
        m_currentInput.keycode = AKEY_RETURN; // Atari RETURN key code
        // Enter key
    } else if (key == Qt::Key_F2) {
        // F2 = Start
        m_currentInput.start = 1;
    } else if (key == Qt::Key_F3) {
        // F3 = Select
        m_currentInput.select = 1;
    } else if (key == Qt::Key_F4) {
        // F4 = Option
        m_currentInput.option = 1;
    } else if (key == Qt::Key_F5 && shiftPressed) {
        // Shift+F5 = Cold Reset (Power) - libatari800 does: lastkey = -input->special
        m_currentInput.special = -AKEY_COLDSTART;  // Convert -3 to +3
    } else if (key == Qt::Key_F5 && !shiftPressed) {
        // F5 = Warm Reset - libatari800 does: lastkey = -input->special  
        m_currentInput.special = -AKEY_WARMSTART;  // Convert -2 to +2
    } else if (key == Qt::Key_F6) {
        // F6 = Help
        m_currentInput.keycode = AKEY_HELP;
    } else if (key == Qt::Key_F7 || key == Qt::Key_Pause) {
        // F7 or Pause = Break - libatari800 does: lastkey = -input->special
        m_currentInput.special = -AKEY_BREAK;  // Convert -5 to +5
    } else if (key == Qt::Key_Exclam) {
        m_currentInput.keychar = '!';
        // Special character: !
    } else if (key == Qt::Key_At) {
        m_currentInput.keychar = '@';
        // Special character: @
    } else if (key == Qt::Key_NumberSign) {
        m_currentInput.keychar = '#';
        // Special character: #
    } else if (key == Qt::Key_Dollar) {
        m_currentInput.keychar = '$';
        // Special character: $
    } else if (key == Qt::Key_Percent) {
        m_currentInput.keychar = '%';
        // Special character: %
    } else if (key == Qt::Key_AsciiCircum) {
        m_currentInput.keychar = '^';
        // Special character: ^
    } else if (key == Qt::Key_Ampersand) {
        m_currentInput.keychar = '&';
        // Special character: &
    } else if (key == Qt::Key_Asterisk) {
        m_currentInput.keychar = '*';
        // Special character: *
    } else if (key == Qt::Key_ParenLeft) {
        m_currentInput.keychar = '(';
        // Special character: (
    } else if (key == Qt::Key_ParenRight) {
        m_currentInput.keychar = ')';
        // Special character: )
    } else if (key == Qt::Key_Question) {
        m_currentInput.keychar = '?';
        // Special character: ?
    } else if (key == Qt::Key_Colon) {
        m_currentInput.keychar = ':';
        // Special character: :
    } else if (key == Qt::Key_Plus) {
        m_currentInput.keychar = '+';
    } else if (key == Qt::Key_Less) {
        m_currentInput.keychar = '<';
    } else if (key == Qt::Key_Underscore) {
        m_currentInput.keychar = '_';
    } else if (key == Qt::Key_Greater) {
        m_currentInput.keychar = '>';
    } else if (key == Qt::Key_QuoteDbl) {
        m_currentInput.keychar = '"';
    } else if (key == Qt::Key_BraceLeft) {
        m_currentInput.keychar = '{';
    } else if (key == Qt::Key_Bar) {
        m_currentInput.keychar = '|';
    } else if (key == Qt::Key_BraceRight) {
        m_currentInput.keychar = '}';
    } else if (key == Qt::Key_AsciiTilde) {
        m_currentInput.keychar = '~';
    } else {
        // Handle other shifted symbols and regular punctuation
        char symbol = getShiftedSymbol(key, shiftPressed);
        if (symbol != 0) {
            m_currentInput.keychar = symbol;
            // Symbol character
        } else {
            // Handle regular punctuation and special keys
            switch (key) {
                case Qt::Key_Semicolon:
                    m_currentInput.keychar = ';';
                    // Semicolon
                    break;
                case Qt::Key_Equal:
                    m_currentInput.keychar = '=';
                    // Equals
                    break;
                case Qt::Key_Comma:
                    m_currentInput.keychar = ',';
                    // Comma
                    break;
                case Qt::Key_Minus:
                    m_currentInput.keychar = '-';
                    // Minus
                    break;
                case Qt::Key_Period:
                    m_currentInput.keychar = '.';
                    // Period
                    break;
                case Qt::Key_Slash:
                    m_currentInput.keychar = '/';
                    // Slash
                    break;
                case Qt::Key_Apostrophe:
                    m_currentInput.keychar = '\'';
                    break;
                case Qt::Key_QuoteLeft:
                    m_currentInput.keychar = '`';
                    break;
                case Qt::Key_BracketLeft:
                    m_currentInput.keychar = '[';
                    break;
                case Qt::Key_BracketRight:
                    m_currentInput.keychar = ']';
                    break;
                case Qt::Key_Backslash:
                    m_currentInput.keychar = '\\';
                    break;
                default:
                    // For special keys, use keycode
                    unsigned char atariKey = convertQtKeyToAtari(key, modifiers);
                    if (atariKey != 0) {
                        m_currentInput.keycode = atariKey;
                        // Function key
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
    
    // For regular keyboard input, only clear keyboard-related fields
    // DO NOT clear joystick values as they should persist
    m_currentInput.keychar = 0;
    m_currentInput.keycode = 0;
    m_currentInput.special = 0;
    m_currentInput.shift = 0;
    m_currentInput.control = 0;
    m_currentInput.start = 0;
    m_currentInput.select = 0;
    m_currentInput.option = 0;
    // Leave joystick values (joy0, joy1, trig0, trig1) unchanged
    // Clear keyboard input on key release
}

void AtariEmulator::coldBoot()
{
    Atari800_Coldstart();
}

void AtariEmulator::warmBoot()
{
    Atari800_Warmstart();
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

QString AtariEmulator::quotePath(const QString& path)
{
    if (path.isEmpty()) {
        return path;
    }

    // First, strip any existing quotes that might have been added by settings or file dialogs
    QString cleanPath = path;
    if (cleanPath.startsWith('"') && cleanPath.endsWith('"')) {
        cleanPath = cleanPath.mid(1, cleanPath.length() - 2);
    }

    // If path contains spaces, quote it to prevent parsing issues
    // This is needed across all platforms (Windows, macOS, Linux)
    if (cleanPath.contains(' ')) {
        // Use double quotes for cross-platform compatibility
        return QString("\"%1\"").arg(cleanPath);
    }

    return cleanPath;
}

void AtariEmulator::setupAudio()
{
#ifdef HAVE_SDL2_AUDIO
    // Unified Audio Backend (preferred when SDL2 is available)
    if (m_audioBackend == UnifiedAudio) {

        // Get audio parameters from libatari800
        int sampleRate = libatari800_get_sound_frequency();
        int channels = libatari800_get_num_sound_channels();
        int sampleSize = libatari800_get_sound_sample_size();


        // Create unified audio backend if not created
        if (!m_unifiedAudio) {
            m_unifiedAudio = new UnifiedAudioBackend(this);
        }

        // Initialize with optimal settings for each platform
        if (m_unifiedAudio->initialize(sampleRate, channels, sampleSize)) {
            return;  // Successfully initialized, skip Qt audio setup
        } else {
            qWarning() << "Failed to initialize Unified Audio Backend, falling back to Qt audio";
            m_audioBackend = QtAudio;
            // Fall through to Qt audio setup
        }
    }
#endif
#ifdef HAVE_SDL2_AUDIO
    if (m_audioBackend == SDL2Audio) {
        
        // Get audio parameters from libatari800
        int sampleRate = libatari800_get_sound_frequency();
        int channels = libatari800_get_num_sound_channels();
        int sampleSize = libatari800_get_sound_sample_size();
        
        
        // Get actual sound buffer to see real size
        unsigned char* testBuffer = libatari800_get_sound_buffer();
        int actualBufferLen = libatari800_get_sound_buffer_len();
        
        // Calculate expected bytes per frame based on sample rate
        // At 44100Hz: 44100 * 2 / 59.92 = 1472 bytes/frame
        // At 22050Hz: 22050 * 2 / 59.92 = 736 bytes/frame  
        int bytesPerFrame = (sampleRate * sampleSize * channels) / 60;  // Approximate
        
        // Create SDL2 audio backend if not created
        if (!m_sdl2Audio) {
            m_sdl2Audio = new SDL2AudioBackend(this);
        }
        
        // Initialize ring buffer with prefill for stability
        m_sdl2AudioBuffer.resize(SDL2_BUFFER_SIZE);
        m_sdl2AudioBuffer.fill(0);
        m_sdl2WritePos = 0;
        m_sdl2ReadPos = 0;
        
        // Set target buffer level based on sample rate
        // Lower sample rates need proportionally smaller buffers
        m_sdl2TargetBufferLevel = bytesPerFrame * 3;  // About 3 frames worth
        m_sdl2BufferLevelAccum = 0;
        m_sdl2BufferLevelCount = 0;
        
        // Minimal prefill to prevent initial underruns
        // Don't overfill as it contributes to accumulation
        int prefillBytes = 1024;  // Just 1 SDL callback worth
        for (int i = 0; i < prefillBytes; i++) {
            m_sdl2AudioBuffer[m_sdl2WritePos] = 0;
            m_sdl2WritePos = (m_sdl2WritePos + 1) % SDL2_BUFFER_SIZE;
        }
        
        // Initialize SDL2 audio
        if (m_sdl2Audio->initialize(sampleRate, channels, sampleSize)) {
            // Set the audio callback to read from our ring buffer
            m_sdl2Audio->setAudioCallback([this](unsigned char* stream, int len) {
                QMutexLocker locker(&m_sdl2AudioMutex);
                
                // Calculate available data in ring buffer
                int availableData = (m_sdl2WritePos - m_sdl2ReadPos + SDL2_BUFFER_SIZE) % SDL2_BUFFER_SIZE;
                
                // Track buffer level for monitoring
                m_sdl2BufferLevelAccum += availableData;
                m_sdl2BufferLevelCount++;
                
                // Report average buffer level periodically
                if (m_sdl2BufferLevelCount >= 100) {
                    int avgLevel = m_sdl2BufferLevelAccum / m_sdl2BufferLevelCount;
                    float percentFull = (float)avgLevel / (float)m_sdl2TargetBufferLevel * 100.0f;
                    
                    // Only report when significantly off target to reduce log spam
                    if (percentFull < 70.0f || percentFull > 150.0f) {
                    }
                    
                    m_sdl2BufferLevelAccum = 0;
                    m_sdl2BufferLevelCount = 0;
                }
                
                if (availableData >= len) {
                    // We have enough data
                    for (int i = 0; i < len; i++) {
                        stream[i] = m_sdl2AudioBuffer[m_sdl2ReadPos];
                        m_sdl2ReadPos = (m_sdl2ReadPos + 1) % SDL2_BUFFER_SIZE;
                    }
                } else if (availableData > 0) {
                    // Partial data available - underrun
                    int i;
                    for (i = 0; i < availableData; i++) {
                        stream[i] = m_sdl2AudioBuffer[m_sdl2ReadPos];
                        m_sdl2ReadPos = (m_sdl2ReadPos + 1) % SDL2_BUFFER_SIZE;
                    }
                    // Fill rest with silence
                    memset(stream + i, 0, len - i);
                    
                    static int underrunCount = 0;
                    underrunCount++;
                    if (underrunCount <= 10 || underrunCount % 100 == 0) {
                    }
                } else {
                    // No data, provide silence
                    memset(stream, 0, len);
                    
                    static int silenceCount = 0;
                    silenceCount++;
                    if (silenceCount <= 10 || silenceCount % 100 == 0) {
                    }
                }
            });
            
            return;  // Successfully initialized, skip Qt audio setup
        } else {
            m_audioBackend = QtAudio;
            // Fall through to Qt audio setup
        }
    }
#endif

    // Qt audio backend setup (original double-buffered implementation)
    if (m_audioBackend == QtAudio) {
    
    // Get audio parameters from libatari800
    m_sampleRate = libatari800_get_sound_frequency();
    int channels = libatari800_get_num_sound_channels();
    int sampleSize = libatari800_get_sound_sample_size();
    m_bytesPerSample = channels * sampleSize;
    
    
    // Setup Qt audio format
    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(channels);
    format.setSampleSize(sampleSize * 8); // Convert bytes to bits
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(sampleSize == 2 ? QAudioFormat::SignedInt : QAudioFormat::UnSignedInt);
    
    // Check if format is supported
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
    if (!info.isFormatSupported(format)) {
        format = info.nearestFormat(format);
    }
    
    
    // Calculate fragment size and buffer parameters (like Atari800MacX)
#ifdef _WIN32
    // Windows Qt audio works best with smaller buffer - add headroom for window operations
    m_fragmentSize = 1024; // Larger fragments for better Qt audio stability (~23ms at 44100Hz)
    int targetDelayMs = 30;   // Slight headroom for UI operations (drag/resize)
#else
    // macOS/Linux can use smaller buffers for lower latency
    m_fragmentSize = 512; // Smaller fragments for more responsive audio
    int targetDelayMs = 40;  // Original delay for macOS/Linux
#endif
    int fragmentBytes = m_fragmentSize * m_bytesPerSample;
    
    // Set up DSP buffer to handle the rate mismatch
    // We generate 1472 bytes/frame but Qt consumes ~940 bytes/call
    // This means we accumulate ~532 bytes per frame (36% excess)
    // Buffer needs to be large enough to absorb this while we wait for consumption
    m_targetDelay = (m_sampleRate * targetDelayMs) / 1000;  // Convert to samples
    // Use a larger buffer to handle the accumulation
#if defined(_WIN32) || defined(__linux__)
    int dspBufferSamples = m_fragmentSize * 20;  // Large buffer for Windows/Linux to handle frame timing variations
#else
    int dspBufferSamples = m_fragmentSize * 10;  // Standard buffer for macOS
#endif
    m_dspBufferBytes = dspBufferSamples * m_bytesPerSample;
    
    // Initialize the DSP buffer
    m_dspBuffer.resize(m_dspBufferBytes);
    m_dspBuffer.fill(0);
    
    // Initialize positions
    m_dspReadPos = 0;
#ifdef _WIN32
    // Windows works best with lower initial buffer matching the working pattern
    m_dspWritePos = (m_targetDelay * 1.0) * m_bytesPerSample;  // Start with target delay (matches 15% pattern)
#else
    m_dspWritePos = m_targetDelay * m_bytesPerSample;  // Start with target delay
#endif
    m_callbackTick = 0;
    m_avgGap = 0.0;
    
    
    // Create and configure audio output
    m_audioOutput = new QAudioOutput(format, this);
    
    // Use platform-optimized Qt buffer size
#if defined(_WIN32) || defined(__linux__)
    m_audioOutput->setBufferSize(8192);  // Large buffer for Windows/Linux to absorb frame timing variations
#else
    m_audioOutput->setBufferSize(2048);  // Balanced buffer for macOS
#endif
    
    // Set notification interval
#if defined(_WIN32) || defined(__linux__)
    // Windows/Linux: Use larger interval for stable audio with dynamic frame timing
    int notifyMs = 50;  // Process audio chunks less frequently
#else
    // macOS: Match frame rate for responsiveness
    int notifyMs = (m_videoSystem == "-ntsc") ? 16 : 20;  // 60Hz or 50Hz
#endif
    m_audioOutput->setNotifyInterval(notifyMs);
    
    // Set category for optimal performance
#ifdef _WIN32
    // Windows: Use music category for higher latency/buffering
    m_audioOutput->setCategory("music");
#else
    // macOS/Linux: Use game category for lower latency
    m_audioOutput->setCategory("game");
#endif
    
    // Set volume to ensure audio is active
    m_audioOutput->setVolume(1.0);
    
    
    // Connect notify signal for audio callback timing
    connect(m_audioOutput, &QAudioOutput::notify, this, [this]() {
        // Update callback tick for gap estimation
        m_callbackTick = QDateTime::currentMSecsSinceEpoch();
    });
    
    m_audioDevice = m_audioOutput->start();
    
    if (m_audioDevice) {
        
        // Test what we actually get from libatari800
        int testBufferLen = libatari800_get_sound_buffer_len();
    } else {
        m_audioEnabled = false;
    }
    }  // End of QtAudio backend setup
}

void AtariEmulator::enableAudio(bool enabled)
{
    if (m_audioEnabled != enabled) {
        m_audioEnabled = enabled;
        
        if (enabled) {
#ifdef HAVE_SDL2_AUDIO
            if (m_audioBackend == UnifiedAudio) {
                if (!m_unifiedAudio) {
                    setupAudio();
                } else {
                    m_unifiedAudio->resume();
                }
            }
#endif
#ifdef HAVE_SDL2_AUDIO
            if (m_audioBackend == SDL2Audio) {
                if (!m_sdl2Audio || !m_sdl2Audio->isInitialized()) {
                    setupAudio();
                } else {
                    m_sdl2Audio->resume();
                }
            }
#endif
            if (!m_audioOutput && m_audioBackend == QtAudio) {
                setupAudio();
            }
        } else {
#ifdef HAVE_SDL2_AUDIO
            if (m_audioBackend == UnifiedAudio && m_unifiedAudio) {
                m_unifiedAudio->pause();
            }
#endif
#ifdef HAVE_SDL2_AUDIO
            if (m_audioBackend == SDL2Audio && m_sdl2Audio) {
                m_sdl2Audio->pause();
            }
#endif
            if (m_audioOutput && m_audioBackend == QtAudio) {
                m_audioOutput->stop();
                delete m_audioOutput;
                m_audioOutput = nullptr;
                m_audioDevice = nullptr;
                // Clear DSP buffer positions
                m_dspReadPos = 0;
                m_dspWritePos = m_targetDelay * m_bytesPerSample;  // Reset to target delay
            }
        }
        
    }
}

void AtariEmulator::setVolume(float volume)
{
#ifdef HAVE_SDL2_AUDIO
    if (m_audioBackend == UnifiedAudio && m_unifiedAudio) {
        m_unifiedAudio->setVolume(volume);
    }
#endif
#ifdef HAVE_SDL2_AUDIO
    if (m_audioBackend == SDL2Audio && m_sdl2Audio) {
        m_sdl2Audio->setVolume(volume);
    }
#endif
    if (m_audioOutput && m_audioBackend == QtAudio) {
        m_audioOutput->setVolume(qBound(0.0f, volume, 1.0f));
    }
}

void AtariEmulator::setAudioBackend(AudioBackend backend)
{
#ifdef HAVE_SDL2_AUDIO
    if (m_audioBackend != backend) {
        // Stop current audio
        if (m_audioEnabled) {
            enableAudio(false);
        }
        
        m_audioBackend = backend;
        
        // Restart audio with new backend
        if (m_audioEnabled) {
            enableAudio(true);
        }
    }
#else
    Q_UNUSED(backend)
    m_audioBackend = QtAudio;
#endif
}

void AtariEmulator::setKbdJoy0Enabled(bool enabled)
{
    m_kbdJoy0Enabled = enabled;
    
#ifdef GUI_SDL
    // Apply the setting to the atari800 core
    // Since we don't have a direct setter, we need to check current state and toggle if needed
    extern int PLATFORM_IsKbdJoystickEnabled(int num);
    extern void PLATFORM_ToggleKbdJoystickEnabled(int num);
    
    bool currentEnabled = PLATFORM_IsKbdJoystickEnabled(0);
    if (currentEnabled != enabled) {
        PLATFORM_ToggleKbdJoystickEnabled(0);
        // Verify the change
        bool newEnabled = PLATFORM_IsKbdJoystickEnabled(0);
    }
#endif
}

void AtariEmulator::setKbdJoy1Enabled(bool enabled)
{
    m_kbdJoy1Enabled = enabled;
    
#ifdef GUI_SDL
    // Apply the setting to the atari800 core
    extern int PLATFORM_IsKbdJoystickEnabled(int num);
    extern void PLATFORM_ToggleKbdJoystickEnabled(int num);
    
    bool currentEnabled = PLATFORM_IsKbdJoystickEnabled(1);
    if (currentEnabled != enabled) {
        PLATFORM_ToggleKbdJoystickEnabled(1);
        // Verify the change
        bool newEnabled = PLATFORM_IsKbdJoystickEnabled(1);
    }
#endif
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

    qDebug() << "PAL color settings updated:"
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

    qDebug() << "NTSC color settings updated:"
             << "Cont:" << COLOURS_NTSC_setup.contrast
             << "Bright:" << COLOURS_NTSC_setup.brightness
             << "Gamma:" << COLOURS_NTSC_setup.gamma
             << "Hue:" << COLOURS_NTSC_setup.hue;
}

void AtariEmulator::updateArtifactSettings(const QString& artifactMode)
{
    
    // Map string mode to ARTIFACT_t enum
    ARTIFACT_t mode = ARTIFACT_NONE;
    
    if (artifactMode == "none") {
        mode = ARTIFACT_NONE;
    } else if (artifactMode == "ntsc-old") {
        mode = ARTIFACT_NTSC_OLD;
    } else if (artifactMode == "ntsc-new") {
        mode = ARTIFACT_NTSC_NEW;
    } else {
        mode = ARTIFACT_NONE;
    }
    
    // Apply the artifact setting immediately
    ARTIFACT_Set(mode);
}

// FUTURE: Scanlines method (commented out - not working)
// bool AtariEmulator::needsScanlineRestart() const
// {
//     // In libatari800, scanlines require restart since no SDL runtime control
//     return true;
// }

void AtariEmulator::setEmulationSpeed(int percentage)
{
    // Set the speed:
    // 0 = unlimited/host speed (maximum turbo)
    // 100 = normal Atari speed
    // Other values = percentage speed multiplier

    // Convert percentage to multiplier and store
    if (percentage == 0) {
        // Host speed (unlimited)
        m_userRequestedSpeedMultiplier = 0.0;  // Special value for unlimited
        Atari800_turbo = 1;
        Atari800_turbo_speed = 0;  // 0 means unlimited in atari800 core
    } else if (percentage < 0) {
        // Invalid input, set to normal speed (100%)
        m_userRequestedSpeedMultiplier = 1.0;
        Atari800_turbo = 0;
        Atari800_turbo_speed = 100;
    } else if (percentage == 100) {
        // Normal Atari speed
        m_userRequestedSpeedMultiplier = 1.0;
        Atari800_turbo = 0;
        Atari800_turbo_speed = 100;
    } else {
        // Custom speed percentage - convert to multiplier (e.g., 200% = 2.0x, 50% = 0.5x)
        m_userRequestedSpeedMultiplier = percentage / 100.0;
        Atari800_turbo = 1;
        Atari800_turbo_speed = percentage;
    }

    // Adjust frame timer interval based on speed
    // Update interval whether timer is active or not (for pause/resume and settings changes)
    if (m_frameTimer) {
        if (m_userRequestedSpeedMultiplier == 0.0) {
            // Unlimited speed - run as fast as possible with minimal interval
            m_frameTimer->setInterval(1);
        } else {
            // Calculate interval based on speed multiplier
            // Faster speed = shorter interval
            // e.g., 2x speed = half interval, 0.5x speed = double interval
            int newInterval = static_cast<int>(m_frameTimeMs / m_userRequestedSpeedMultiplier);
            m_frameTimer->setInterval(newInterval);
        }
    }

    qDebug() << "Speed set to" << percentage << "% (multiplier:" << m_userRequestedSpeedMultiplier
             << ") - Atari800_turbo:" << Atari800_turbo
             << "Atari800_turbo_speed:" << Atari800_turbo_speed
             << "Frame interval:" << (m_frameTimer ? m_frameTimer->interval() : 0) << "ms";
}

int AtariEmulator::getCurrentEmulationSpeed() const
{
    // Return the current emulation speed percentage
    // Note: 0 means unlimited/host speed
    if (Atari800_turbo && Atari800_turbo_speed == 0) {
        return 0;  // Host speed (unlimited)
    }
    return Atari800_turbo_speed;
}


bool AtariEmulator::loadXexForDebug(const QString& filename)
{
    // Start the XEX loading process
    if (!libatari800_reboot_with_file(filename.toUtf8().constData())) {
        return false;
    }
    
    // Get memory pointer for checking vectors
    unsigned char* mem = libatari800_get_main_memory_ptr();
    if (!mem) {
        return false;
    }
    
    // Step frames until loading completes (max 120 frames = 2 seconds at 60fps)
    const int maxFrames = 120;
    int framesProcessed = 0;
    bool loadingComplete = false;
    
    
    while (framesProcessed < maxFrames) {
        // Process one frame to advance the loading
        processFrame();
        framesProcessed++;
        
        // Check if loading has completed
        // BINLOAD_start_binloading is set to TRUE when loading starts and FALSE when done
        if (!BINLOAD_start_binloading) {
            // Also verify RUNAD or INITAD is set
            unsigned short runad = mem[0x2E0] | (mem[0x2E1] << 8);
            unsigned short initad = mem[0x2E2] | (mem[0x2E3] << 8);
            
            if ((runad != 0x0000 && runad != 0xFFFF) || 
                (initad != 0x0000 && initad != 0xFFFF)) {
                loadingComplete = true;
                break;
            }
        }
        
        // For initial frames, BINLOAD_start_binloading might not be set yet
        // So also check if vectors become valid
        if (framesProcessed > 10) {
            unsigned short runad = mem[0x2E0] | (mem[0x2E1] << 8);
            unsigned short initad = mem[0x2E2] | (mem[0x2E3] << 8);
            
            if ((runad != 0x0000 && runad != 0xFFFF) || 
                (initad != 0x0000 && initad != 0xFFFF)) {
                // Vectors are set, check if we're past the loader
                if (CPU_regPC < 0xD000 || CPU_regPC >= 0xE000) {
                    // PC is not in ROM loader area, likely done
                    loadingComplete = true;
                    break;
                }
            }
        }
    }
    
    // Pause execution now that loading is complete (or timeout)
    pauseEmulation();
    
    // Read the entry point vectors
    unsigned short runad = mem[0x2E0] | (mem[0x2E1] << 8);
    unsigned short initad = mem[0x2E2] | (mem[0x2E3] << 8);
    
    // Determine the entry point
    unsigned short entryPoint;
    if (runad != 0x0000 && runad != 0xFFFF) {
        entryPoint = runad;
    } else if (initad != 0x0000 && initad != 0xFFFF) {
        entryPoint = initad;
    } else {
        // Fallback to current PC if no vectors set (shouldn't happen)
        entryPoint = CPU_regPC;
    }
    
    if (!loadingComplete) {
        qDebug() << "Binary loaded with entry point: $"
                 << QString("%1").arg(entryPoint, 4, 16, QChar('0')).toUpper()
                 << "Current PC: $"
                 << QString("%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
    }
    
    // Emit signal for debugger widget to handle (could set a temporary breakpoint)
    emit xexLoadedForDebug(entryPoint);
    
    return true;
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
            case 'L': m_currentInput.keychar = 'l'; break;
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
            case 'l': m_currentInput.keychar = 'l'; break;
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
    } else if (ch == '=') {
        // Equals sign
        m_currentInput.keychar = '=';
    } else if (ch == '+') {
        // Plus sign
        m_currentInput.keychar = '+';
    } else if (ch == '-') {
        // Minus sign
        m_currentInput.keychar = '-';
    } else if (ch == '*') {
        // Asterisk
        m_currentInput.keychar = '*';
    } else if (ch == '/') {
        // Slash
        m_currentInput.keychar = '/';
    } else if (ch == '(') {
        // Left parenthesis
        m_currentInput.keychar = '(';
    } else if (ch == ')') {
        // Right parenthesis
        m_currentInput.keychar = ')';
    } else if (ch == ',') {
        // Comma
        m_currentInput.keychar = ',';
    } else if (ch == ';') {
        // Semicolon
        m_currentInput.keychar = ';';
    } else if (ch == ':') {
        // Colon
        m_currentInput.keychar = ':';
    } else if (ch == '.') {
        // Period
        m_currentInput.keychar = '.';
    } else if (ch == '?') {
        // Question mark
        m_currentInput.keychar = '?';
    } else if (ch == '!') {
        // Exclamation mark
        m_currentInput.keychar = '!';
    } else if (ch == '<') {
        // Less than
        m_currentInput.keychar = '<';
    } else if (ch == '>') {
        // Greater than
        m_currentInput.keychar = '>';
    } else if (ch == '\'') {
        // Apostrophe
        m_currentInput.keychar = '\'';
    } else {
        return;
    }
    
    // IMPROVED FIX: Let the input persist for multiple frames to ensure proper processing
    // The main emulation loop will process this input over several frames
    // Don't clear immediately - let the regular frame processing handle it
    
    // Process a few frames to ensure the character is registered
    for (int i = 0; i < 3; i++) {
        libatari800_next_frame(&m_currentInput);
    }
    
    // Now clear the input after multiple frame processing
    libatari800_clear_input_array(&m_currentInput);
    // Restore joystick center positions after clearing
    m_currentInput.joy0 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.joy1 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
}

void AtariEmulator::injectAKey(int akeyCode)
{
    // Clear previous input
    libatari800_clear_input_array(&m_currentInput);
    // Restore joystick center positions after clearing (using libatari800-compatible center)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.joy1 = 0x0f ^ 0xff;  // INPUT_STICK_CENTRE for libatari800
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
    
    // Directly set the raw AKEY code
    m_currentInput.keycode = akeyCode;
    
    
    // Schedule key release after a short delay (one frame)
    QTimer::singleShot(50, this, [this]() {
        libatari800_clear_input_array(&m_currentInput);
        // Restore joystick center positions after clearing
        m_currentInput.joy0 = 0x0f ^ 0xff;
        m_currentInput.joy1 = 0x0f ^ 0xff;
        m_currentInput.trig0 = 0;
        m_currentInput.trig1 = 0;
    });
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
        emit executionPaused();
    }
}

void AtariEmulator::resumeEmulation()
{
    if (m_emulationPaused) {
        m_frameTimer->start();
        m_emulationPaused = false;
        // Reset last PC when resuming to avoid missing breakpoints
        m_lastPC = 0xFFFF;
        emit executionResumed();
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
        emit debugStepped();
    } else {
    }
}

void AtariEmulator::stepOneInstruction()
{
    if (m_emulationPaused) {
        // Without patches, we can't do true single-instruction stepping
        // We'll use frame stepping as an approximation
        // This executes many instructions but is the best we can do without patches
        unsigned short startPC = CPU_regPC;
        
        // Execute one frame
        // This will execute thousands of instructions, but it's all we have
        libatari800_next_frame(&m_currentInput);
        
        // Check breakpoints after execution
        checkBreakpoints();

        qDebug() << "Debug step completed: PC from $"
                 << QString("%1").arg(startPC, 4, 16, QChar('0')).toUpper()
                 << "to $"
                 << QString("%1").arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
        emit debugStepped();
    } else {
    }
}

double AtariEmulator::calculateSpeedAdjustment()
{
    // Calculate buffer gap similar to Atari800MacX
    int gap = 0;
    
    if (m_audioBackend == QtAudio && m_audioEnabled && m_audioOutput) {
        // Calculate how much data is buffered
        int bufferedBytes = (m_dspWritePos - m_dspReadPos + m_dspBufferBytes) % m_dspBufferBytes;
        int bufferedSamples = bufferedBytes / m_bytesPerSample;
        
        // Calculate gap from target delay
        gap = m_targetDelay - bufferedSamples;
        
        // Update smoothed average gap
        m_avgGap = m_avgGap * (1.0 - SPEED_ADJUSTMENT_ALPHA) + gap * SPEED_ADJUSTMENT_ALPHA;
        
        // Calculate speed adjustment based on gap
        // Positive gap = buffer running low, speed up
        // Negative gap = buffer too full, slow down
        double speedAdjustment = 0.0;
        
#ifdef _WIN32
        // Windows: Use gentler speed adjustments with larger buffer thresholds
        if (m_avgGap > 150) {
            // Buffer very low, speed up moderately
            speedAdjustment = 0.03;
        } else if (m_avgGap > 60) {
            // Buffer low, speed up slightly
            speedAdjustment = 0.015;
        } else if (m_avgGap < -150) {
            // Buffer very full, slow down moderately
            speedAdjustment = -0.03;
        } else if (m_avgGap < -60) {
            // Buffer full, slow down slightly
            speedAdjustment = -0.015;
        }
#else
        // macOS/Linux: Original more aggressive adjustments
        if (m_avgGap > 50) {
            // Buffer very low, speed up significantly
            speedAdjustment = 0.05;
        } else if (m_avgGap > 20) {
            // Buffer low, speed up slightly
            speedAdjustment = 0.02;
        } else if (m_avgGap < -50) {
            // Buffer very full, slow down significantly
            speedAdjustment = -0.05;
        } else if (m_avgGap < -20) {
            // Buffer full, slow down slightly
            speedAdjustment = -0.02;
        }
#endif
        
        return speedAdjustment;
    }
    
    return 0.0;
}

void AtariEmulator::updateEmulationSpeed()
{
    // Don't apply audio sync adjustments if user requested unlimited speed
    if (m_userRequestedSpeedMultiplier == 0.0) {
        return;  // Keep unlimited speed, don't adjust
    }

    if (!m_audioEnabled || m_audioBackend != QtAudio) {
        m_currentSpeed = 1.0;
        return;
    }

    // Calculate speed adjustment for audio synchronization
    double adjustment = calculateSpeedAdjustment();

    // Update target speed (audio sync adjustment only)
    m_targetSpeed = 1.0 + adjustment;

    // Clamp to reasonable range (95% to 105%)
    m_targetSpeed = qBound(0.95, m_targetSpeed, 1.05);

    // Smooth transition to target speed
#ifdef _WIN32
    // Windows: Smoother transition to avoid audio artifacts
    m_currentSpeed = m_currentSpeed * 0.95 + m_targetSpeed * 0.05;
#else
    // macOS/Linux: Faster transition for more responsive adjustment
    m_currentSpeed = m_currentSpeed * 0.9 + m_targetSpeed * 0.1;
#endif

    // Apply combined speed to frame timer:
    // Base speed from user request, fine-tuned by audio sync adjustment
    if (m_frameTimer) {
        // Combine user-requested speed with audio sync adjustment
        double combinedSpeed = m_userRequestedSpeedMultiplier * m_currentSpeed;

        // Adjust frame interval based on combined speed
        // Faster speed = shorter interval
        float adjustedFrameTime = m_frameTimeMs / combinedSpeed;
        m_frameTimer->setInterval(static_cast<int>(adjustedFrameTime));
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
                 << "inDiskRoutine=" << inDiskRoutine << "mountedDrives=" << m_mountedDrives.size();
        debugCounter = 0;
    }
    
    static bool wasInDiskRoutine = false;
    static int activeDrive = -1;
    
    if (inDiskRoutine && !wasInDiskRoutine) {
        // Just entered disk routine - turn LED ON
        
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
        
                 << "Drive D" << driveNumber << ":" << (isWriting ? "WRITE" : "READ")
                 << "PC:" << QString("$%1").arg(currentPC, 4, 16, QChar('0')).toUpper()
                 << "Mounted drives:" << m_mountedDrives;
        
        emit diskIOStart(driveNumber, isWriting);
        
    } else if (!inDiskRoutine && wasInDiskRoutine && activeDrive != -1) {
        // Exited disk routine - turn LED OFF
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
    
    // Joystick key event processing
    
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
                    } else {
                        joy1_up = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
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
                    } else {
                        joy1_down = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
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
                    } else {
                        joy1_left = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
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
                    } else {
                        joy1_right = isKeyPress;
                        m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
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
                } else {
                    trig0State = false;
                    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
                }
            } else {
                if (isKeyPress) {
                    trig1State = true;
                    m_currentInput.trig1 = 1;  // 1 = pressed (inverted for libatari800)
                } else {
                    trig1State = false;
                    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
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
                } else {
                    joy1_up = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                }
                return true;
                
            case Qt::Key_S:         // S - DOWN
                if (wasdTargetJoy == 0) {
                    joy0_down = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                } else {
                    joy1_down = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                }
                return true;
                
            case Qt::Key_A:         // A - LEFT
                if (wasdTargetJoy == 0) {
                    joy0_left = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                } else {
                    joy1_left = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                }
                return true;
                
            case Qt::Key_D:         // D - RIGHT
                if (wasdTargetJoy == 0) {
                    joy0_right = isKeyPress;
                    m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                } else {
                    joy1_right = isKeyPress;
                    m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                }
                return true;
                
            case Qt::Key_Space:     // Space - FIRE for WASD joystick
                // libatari800 inverts trigger values: returns input->trig0 ? 0 : 1
                // So we send: 1 = pressed, 0 = released (inverted)
                if (wasdTargetJoy == 0) {
                    if (isKeyPress) {
                        trig0State = true;
                        m_currentInput.trig0 = 1;  // 1 = pressed (inverted for libatari800)
                    } else {
                        trig0State = false;
                        m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
                    }
                } else {
                    if (isKeyPress) {
                        trig1State = true;
                        m_currentInput.trig1 = 1;  // 1 = pressed (inverted for libatari800)
                    } else {
                        trig1State = false;
                        m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
                    }
                }
                return true;
                
            // Diagonal keys for WASD joystick - using direct diagonal constants
            case Qt::Key_Q:         // Q - UP+LEFT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UL : INPUT_STICK_CENTRE;
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UL : INPUT_STICK_CENTRE;
                }
                return true;
                
            case Qt::Key_E:         // E - UP+RIGHT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UR : INPUT_STICK_CENTRE;
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UR : INPUT_STICK_CENTRE;
                }
                return true;
                
            case Qt::Key_Z:         // Z - DOWN+LEFT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LL : INPUT_STICK_CENTRE;
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LL : INPUT_STICK_CENTRE;
                }
                return true;
                
            case Qt::Key_C:         // C - DOWN+RIGHT diagonal
                if (wasdTargetJoy == 0) {
                    m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LR : INPUT_STICK_CENTRE;
                } else {
                    m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LR : INPUT_STICK_CENTRE;
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

    qDebug() << "SIO patch changed"
             << "to" << (enabled ? "ENABLED" : "DISABLED");

    return previousState != 0;
}

void AtariEmulator::debugSIOPatchStatus() const
{
    bool isEnabled = getSIOPatchEnabled();

    qDebug() << "SIO Patch Status:" << (isEnabled ? "ENABLED" : "DISABLED");
    qDebug() << (isEnabled ?
        "Disk operations bypass sector delays for faster loading" :
        "Disk operations use realistic timing delays (~3200 scanlines between sectors)");
    
    // Also check compile-time settings
    #ifdef NO_SECTOR_DELAY
    #else
    #endif
}

// Printer support functions
void AtariEmulator::setPrinterEnabled(bool enabled)
{
    if (m_printerEnabled != enabled) {
        m_printerEnabled = enabled;
        
        // Note: P: device setup is now handled during emulator initialization
        // Printer enable/disable changes trigger a full emulator restart via SettingsDialog
    }
}

bool AtariEmulator::isPrinterEnabled() const
{
    return m_printerEnabled;
}

void AtariEmulator::setPrinterOutputCallback(std::function<void(const QString&)> callback)
{
    m_printerOutputCallback = callback;
}

void AtariEmulator::setPrintCommand(const QString& command)
{
    if (!command.isEmpty()) {
        Devices_SetPrintCommand(command.toUtf8().constData());
    }
}

// Joystick control methods for TCP server
void AtariEmulator::setJoystickState(int player, int direction, bool fire)
{
    if (player < 1 || player > 2) {
        qWarning() << "Invalid joystick player:" << player << "- must be 1 or 2";
        return;
    }
    
    // Direction should be pre-inverted value (0-255) or we can accept 0-15 and invert here
    // Let's accept both: if direction <= 15, assume it needs inversion
    int invertedDirection = direction;
    if (direction <= 15) {
        invertedDirection = direction ^ 0xff;  // Invert for libatari800
    }
    
    if (player == 1) {
        m_currentInput.joy0 = invertedDirection;
        m_currentInput.trig0 = fire ? 1 : 0;  // 1 = pressed (inverted for libatari800)
    } else {
        m_currentInput.joy1 = invertedDirection;
        m_currentInput.trig1 = fire ? 1 : 0;  // 1 = pressed (inverted for libatari800)
    }
}

void AtariEmulator::releaseJoystick(int player)
{
    if (player < 1 || player > 2) {
        qWarning() << "Invalid joystick player:" << player << "- must be 1 or 2";
        return;
    }
    
    const int CENTER = 0x0f ^ 0xff;  // Center position inverted for libatari800
    
    if (player == 1) {
        m_currentInput.joy0 = CENTER;
        m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    } else {
        m_currentInput.joy1 = CENTER;
        m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)
    }
}

// Joystick monitoring methods for TCP server
int AtariEmulator::getJoystickState(int player) const
{
    if (player == 1) {
        return m_currentInput.joy0;
    } else if (player == 2) {
        return m_currentInput.joy1;
    }
    return 0x0f ^ 0xff;  // Return center if invalid player
}

bool AtariEmulator::getJoystickFire(int player) const
{
    if (player == 1) {
        return m_currentInput.trig0 == 1;  // 1 = pressed in our inverted logic
    } else if (player == 2) {
        return m_currentInput.trig1 == 1;  // 1 = pressed in our inverted logic
    }
    return false;
}

QJsonObject AtariEmulator::getAllJoystickStates() const
{
    QJsonObject result;
    
    // Helper to convert inverted direction value back to human-readable direction
    auto getDirectionName = [](int value) -> QString {
        // These are the inverted values we use
        const int CENTER = 0x0f ^ 0xff;  // 240
        const int UP = 0x0e ^ 0xff;      // 241
        const int DOWN = 0x0d ^ 0xff;    // 242
        const int LEFT = 0x0b ^ 0xff;    // 244
        const int RIGHT = 0x07 ^ 0xff;   // 248
        const int UP_LEFT = 0x0a ^ 0xff; // 245
        const int UP_RIGHT = 0x06 ^ 0xff; // 249
        const int DOWN_LEFT = 0x09 ^ 0xff; // 246
        const int DOWN_RIGHT = 0x05 ^ 0xff; // 250
        
        switch(value) {
            case CENTER: return "CENTER";
            case UP: return "UP";
            case DOWN: return "DOWN";
            case LEFT: return "LEFT";
            case RIGHT: return "RIGHT";
            case UP_LEFT: return "UP_LEFT";
            case UP_RIGHT: return "UP_RIGHT";
            case DOWN_LEFT: return "DOWN_LEFT";
            case DOWN_RIGHT: return "DOWN_RIGHT";
            default: return QString("UNKNOWN_%1").arg(value);
        }
    };
    
    // Joystick 1
    QJsonObject joy1;
    joy1["direction"] = getDirectionName(m_currentInput.joy0);
    joy1["direction_value"] = m_currentInput.joy0;
    joy1["fire"] = (m_currentInput.trig0 == 1);  // 1 = pressed in our inverted logic
    joy1["keyboard_enabled"] = m_kbdJoy0Enabled;
    joy1["keyboard_keys"] = m_swapJoysticks ? "wasd" : "numpad";
    
    // Joystick 2
    QJsonObject joy2;
    joy2["direction"] = getDirectionName(m_currentInput.joy1);
    joy2["direction_value"] = m_currentInput.joy1;
    joy2["fire"] = (m_currentInput.trig1 == 1);  // 1 = pressed in our inverted logic
    joy2["keyboard_enabled"] = m_kbdJoy1Enabled;
    joy2["keyboard_keys"] = m_swapJoysticks ? "numpad" : "wasd";
    
    result["joystick1"] = joy1;
    result["joystick2"] = joy2;
    result["swapped"] = m_swapJoysticks;
    
    return result;
}

bool AtariEmulator::saveState(const QString& filename)
{
    // Pause emulation during save
    bool wasPaused = m_emulationPaused;
    if (!wasPaused) {
        pauseEmulation();
    }
    
    // When using libatari800, state saves work with memory buffers
    // Allocate buffer for state data (STATESAV_MAX_SIZE is defined in libatari800.h)
    UBYTE* stateBuffer = new UBYTE[STATESAV_MAX_SIZE];
    
    // Set up the global buffer pointer that libatari800 expects
    extern UBYTE* LIBATARI800_StateSav_buffer;
    extern statesav_tags_t* LIBATARI800_StateSav_tags;
    LIBATARI800_StateSav_buffer = stateBuffer;
    
    // Create tags structure
    statesav_tags_t tags;
    memset(&tags, 0, sizeof(tags));
    LIBATARI800_StateSav_tags = &tags;
    
    // Save state to buffer
    extern void LIBATARI800_StateSave(UBYTE *buffer, statesav_tags_t *tags);
    LIBATARI800_StateSave(stateBuffer, &tags);
    
    // Get actual size of saved data
    int stateSize = tags.size;
    if (stateSize == 0) {
        // Fall back to max size if size tag wasn't set
        stateSize = STATESAV_MAX_SIZE;
    }
    
    // Write buffer to file
    QFile file(filename);
    bool success = false;
    if (file.open(QIODevice::WriteOnly)) {
        qint64 written = file.write(reinterpret_cast<const char*>(stateBuffer), stateSize);
        success = (written == stateSize);
        file.close();
    }
    
    // Clean up
    delete[] stateBuffer;
    LIBATARI800_StateSav_buffer = nullptr;
    LIBATARI800_StateSav_tags = nullptr;
    
    // Resume if we weren't paused before
    if (!wasPaused) {
        resumeEmulation();
    }
    
    if (success) {
        // Save metadata
        QSettings stateSettings(filename + ".meta", QSettings::IniFormat);
        stateSettings.setValue("profile", m_currentProfileName);
        stateSettings.setValue("timestamp", QDateTime::currentDateTime());
        
        return true;
    } else {
        qWarning() << "Failed to save state to:" << filename;
        return false;
    }
}

bool AtariEmulator::loadState(const QString& filename)
{
    // Check if state file exists
    QFileInfo stateFile(filename);
    if (!stateFile.exists()) {
        qWarning() << "State file does not exist:" << filename;
        return false;
    }
    
    // Pause emulation during load
    bool wasPaused = m_emulationPaused;
    if (!wasPaused) {
        pauseEmulation();
    }
    
    // Read state file into buffer
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open state file:" << filename;
        if (!wasPaused) {
            resumeEmulation();
        }
        return false;
    }
    
    QByteArray fileData = file.readAll();
    file.close();
    
    if (fileData.isEmpty()) {
        qWarning() << "State file is empty:" << filename;
        if (!wasPaused) {
            resumeEmulation();
        }
        return false;
    }
    
    // Set up the global buffer pointer that libatari800 expects
    extern UBYTE* LIBATARI800_StateSav_buffer;
    LIBATARI800_StateSav_buffer = reinterpret_cast<UBYTE*>(fileData.data());
    
    // Load state from buffer
    extern void LIBATARI800_StateLoad(UBYTE *buffer);
    LIBATARI800_StateLoad(reinterpret_cast<UBYTE*>(fileData.data()));
    
    // Clean up
    LIBATARI800_StateSav_buffer = nullptr;
    
    // Resume if we weren't paused before
    if (!wasPaused) {
        resumeEmulation();
    }
    
    // Load the profile name from meta file if it exists
    QString metaFile = filename + ".meta";
    if (QFileInfo::exists(metaFile)) {
        QSettings stateSettings(metaFile, QSettings::IniFormat);
        QString profileName = stateSettings.value("profile").toString();
        if (!profileName.isEmpty()) {
            m_currentProfileName = profileName;
        }
    }
    
    return true;
}

bool AtariEmulator::quickSaveState()
{
    QString quickSavePath = getQuickSaveStatePath();
    return saveState(quickSavePath);
}

bool AtariEmulator::quickLoadState()
{
    QString quickSavePath = getQuickSaveStatePath();
    if (!QFileInfo::exists(quickSavePath)) {
        qWarning() << "Quick save state does not exist";
        return false;
    }
    return loadState(quickSavePath);
}

QString AtariEmulator::getQuickSaveStatePath() const
{
    // Use the application's data directory for quick saves
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("quicksave.a8s");
}

// Breakpoint management - core debugging support
void AtariEmulator::addBreakpoint(unsigned short address)
{
    if (!m_breakpoints.contains(address)) {
        m_breakpoints.insert(address);
        emit breakpointAdded(address);
    }
}

void AtariEmulator::removeBreakpoint(unsigned short address)
{
    if (m_breakpoints.remove(address)) {
        emit breakpointRemoved(address);
    }
}

void AtariEmulator::clearAllBreakpoints()
{
    if (!m_breakpoints.isEmpty()) {
        m_breakpoints.clear();
        emit breakpointsCleared();
    }
}

bool AtariEmulator::hasBreakpoint(unsigned short address) const
{
    return m_breakpoints.contains(address);
}

QSet<unsigned short> AtariEmulator::getBreakpoints() const
{
    return m_breakpoints;
}

void AtariEmulator::setBreakpointsEnabled(bool enabled)
{
    m_breakpointsEnabled = enabled;
}

bool AtariEmulator::areBreakpointsEnabled() const
{
    return m_breakpointsEnabled;
}

void AtariEmulator::checkBreakpoints()
{
    if (!m_breakpointsEnabled || m_breakpoints.isEmpty() || m_emulationPaused) {
        return;
    }
    
    unsigned short currentPC = CPU_regPC;
    
    // Only check if PC has changed (avoid repeated triggers at same address)
    if (currentPC != m_lastPC) {
        m_lastPC = currentPC;
        
        if (m_breakpoints.contains(currentPC)) {
            pauseEmulation();
            emit breakpointHit(currentPC);
        }
    }
}

#ifdef HAVE_SDL2_JOYSTICK
void AtariEmulator::setRealJoysticksEnabled(bool enabled)
{
    if (m_realJoysticksEnabled == enabled) {
        return;
    }

    m_realJoysticksEnabled = enabled;

    if (m_joystickManager) {
        m_joystickManager->setEnabled(enabled);
    }
}

void AtariEmulator::setJoystick1Device(const QString& device)
{
    m_joystick1AssignedDevice = device;
}

void AtariEmulator::setJoystick2Device(const QString& device)
{
    m_joystick2AssignedDevice = device;
}
#endif

// Double buffering audio implementation (inspired by Atari800MacX)
