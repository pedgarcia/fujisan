/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifdef _WIN32
#include "windows_compat.h"
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "atariemulator.h"
#include <QDebug>
#include <QApplication>
#include <QMetaObject>
#include <QTimer>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QCoreApplication>
#include <QEvent>
#include <QByteArray>
#include <cstring>  // for memset
#include <vector>   // for std::vector
#include <chrono>   // for high-resolution logging timestamps

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
// NetSIO enabled flag - set by atari800 core when FujiNet-PC confirms connection
extern volatile int netsio_enabled;
// NetSIO reset functions - send 0xFF/0xFE packets to FujiNet-PC
extern int netsio_cold_reset(void);
extern int netsio_warm_reset(void);
#ifdef _WIN32
extern void netsio_flush_fifo(void);
#endif
#endif
#include "../src/rtime.h"
#include "../src/binload.h"
// Printer support functions
void ESC_PatchOS(void);
void Devices_UpdatePatches(void);
// H: device patch flag - disabled by -netsio, must re-enable for H: drives
extern int Devices_enable_h_patch;
// H: device directory paths (indexed 0-3 for H1:-H4:)
extern char Devices_atari_h_dir[4][FILENAME_MAX];

// Access to CPU registers for XEX loading
extern unsigned char CPU_regS;
extern unsigned short CPU_regPC;

// Access to GTIA console override for warm boot BASIC state preservation
extern int GTIA_consol_override;

// Direct Atari RAM access — used to poll CH ($02FC) for OS keyboard readiness.
unsigned char *libatari800_get_main_memory_ptr();
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
            if (!s_emulatorInstance || !s_emulatorInstance->isLibatari800Initialized())
                return;
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
    , m_netSIOEnabled(false)
    , m_usePartialFrameExecution(false)
    , m_cyclesThisFrame(0)
#ifdef HAVE_SDL2_JOYSTICK
    , m_joystickManager(nullptr)
    , m_realJoysticksEnabled(false)
    , m_joystick1AssignedDevice("keyboard")
    , m_joystick2AssignedDevice("keyboard")
#endif
{
    // libatari800 is not initialized yet; only reset our template (see clearCurrentInputLocked).
    std::memset(&m_currentInput, 0, sizeof(m_currentInput));

    // Initialize joysticks to center position (inverted for libatari800)
    m_currentInput.joy0 = 0x0f ^ 0xff;  // 15 ^ 255 = 240
    m_currentInput.joy1 = 0x0f ^ 0xff;  // 15 ^ 255 = 240
    m_currentInput.trig0 = 0;  // 0 = released (inverted for libatari800)
    m_currentInput.trig1 = 0;  // 0 = released (inverted for libatari800)

    // Set up the global instance pointer for the callback
    s_emulatorInstance = this;
    
    // Use single-shot PreciseTimer for drift-compensated frame scheduling.
    // Single-shot mode means it fires exactly once per requestNextFrame() call,
    // preventing duplicate firings when the emulator is restarted mid-flight.
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    m_frameTimer->setSingleShot(true);
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
    if (s_emulatorInstance == this) {
        s_emulatorInstance = nullptr;
        libatari800_set_disk_activity_callback(nullptr);
    }
    shutdown();
    teardownAudio();
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
                // CRITICAL: Must set -rev AUTO BEFORE the ROM file to reset any previously cached revision
                // This clears the Altirra OS setting and allows libatari800 to auto-select the custom ROM
                // NOTE: Must use uppercase "AUTO" - atari800 uses case-sensitive strcmp() for this value
                // NOTE: Do NOT use quotePath() here! We're passing arguments via char* array (argc/argv),
                // not as a shell command string. Quotes are only needed for shell parsing.
                // Each QStringList element becomes a separate argv[] entry, so spaces are handled correctly.
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "AUTO";
                    argList << "-5200_rom" << m_osRomPath;
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "AUTO";
                    argList << "-osb_rom" << m_osRomPath;  // 800 OS-B ROM
                } else {
                    // For XL/XE machines
                    argList << "-xl-rev" << "AUTO";
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

    // Configure BASIC ROM according to user settings
    // This ensures OS disks can access BASIC when needed (e.g., SmartDOS with FujiNet)
    if (m_altirraBASICEnabled) {
        // User explicitly wants Altirra BASIC
        argList << "-basic-rev" << "altirra";
        qDebug() << "  -> Configured Altirra BASIC ROM";
    } else if (!m_basicRomPath.isEmpty()) {
        // User specified an external BASIC ROM
        QFileInfo basicRomFile(m_basicRomPath);
        if (basicRomFile.exists()) {
            // CRITICAL: Must set -basic-rev AUTO BEFORE the ROM file to reset any previously cached revision
            // NOTE: Do NOT quote - we're using char* array, not shell string
            argList << "-basic-rev" << "AUTO";
            argList << "-basic_rom" << m_basicRomPath;
            qDebug() << "  -> Configured external BASIC ROM:" << m_basicRomPath;
        } else {
            qWarning() << "BASIC ROM file not found at" << m_basicRomPath;
            qDebug() << "  -> No BASIC ROM configured";
            // Don't configure any BASIC ROM - just use OS
        }
    } else {
        // No BASIC ROM preference specified - just use OS
        qDebug() << "  -> No BASIC ROM configured - OS only";
        // Don't add any BASIC ROM configuration
    }

    // Separately control whether to boot into BASIC mode
    if (basicEnabled) {
        argList << "-basic";  // Boot into BASIC prompt
        qDebug() << "  -> Boot mode: BASIC prompt";
    } else {
        argList << "-nobasic";  // Boot to OS/DOS (don't go to BASIC prompt)
        qDebug() << "  -> Boot mode: OS/DOS (no BASIC prompt)";
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
        // Match shutdown(): clear flag before exit so deferred Qt timers skip lib calls.
        m_libatari800Initialized = false;
        libatari800_exit();
    } else {
    }
    
    if (libatari800_init(argBytes.size(), args.data())) {
        libatari800_previously_initialized = true;  // Mark as initialized for future resets
        m_libatari800Initialized = true;

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

        // Initialize absolute-time frame scheduler and reset PI state
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_frameCount     = 0;
        m_piIntegral     = 0.0;
        m_piSpeedTrim    = 0.0;
        m_avgGap         = 0.0;

        // Schedule first frame
        requestNextFrame();
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

    // Memory Configuration
    QSettings memSettings("8bitrelics", "Fujisan");

    // Note: enable800Ram (C000-CFFF RAM for Atari 800) doesn't have a direct command-line
    // parameter. For Atari 800, the RAM size is set based on machine type. The 48KB
    // configuration (which includes C000-CFFF) is the default for -atari machine type.
    // If finer control is needed, MEMORY_ram_size extern can be set after initialization.

    // Mosaic RAM expansion (48-1024 KB, in 4KB increments)
    bool enableMosaic = memSettings.value("machine/enableMosaic", false).toBool();

    // Axlon RAM expansion (valid total sizes: 32, 160, 288, 544, 1056 KB)
    bool enableAxlon = memSettings.value("machine/enableAxlon", false).toBool();

    // libatari800 rejects init if both -mosaic and -axlon are passed simultaneously.
    // If both are enabled in settings (misconfiguration), prefer Axlon and skip Mosaic.
    if (enableMosaic && enableAxlon) {
        qWarning() << "Both Mosaic and Axlon RAM expansions are enabled — they are mutually exclusive."
                   << "Disabling Mosaic; only Axlon will be used.";
        enableMosaic = false;
    }

    if (enableMosaic) {
        int mosaicSize = memSettings.value("machine/mosaicSize", 320).toInt();
        argList << "-mosaic" << QString::number(mosaicSize);
    }

    if (enableAxlon) {
        int axlonSize = memSettings.value("machine/axlonSize", 320).toInt();
        argList << "-axlon" << QString::number(axlonSize);

        // Axlon 0xF bank shadow mode
        bool axlonShadow = memSettings.value("machine/axlonShadow", false).toBool();
        if (axlonShadow) {
            argList << "-axlon0f";
        }
    }

    // MapRAM (XL/XE machines only)
    bool enableMapRam = memSettings.value("machine/enableMapRam", true).toBool();
    if (enableMapRam) {
        argList << "-mapram";
    } else {
        argList << "-no-mapram";
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

    // Diagnostic logging: Log emulator state before ROM configuration
    QString platform;
#ifdef Q_OS_WIN
    platform = "Windows";
#elif defined(Q_OS_LINUX)
    platform = "Linux";
#elif defined(Q_OS_MACOS)
    platform = "macOS";
#else
    platform = "Unknown";
#endif

    qDebug() << "=== ROM CONFIGURATION DIAGNOSTIC [Platform:" << platform << "] ===";
    qDebug() << "  m_altirraOSEnabled:" << m_altirraOSEnabled;
    qDebug() << "  m_osRomPath:" << m_osRomPath;
    qDebug() << "  machineType:" << machineType;
    qDebug() << "  basicEnabled:" << basicEnabled;
    qDebug() << "  m_altirraBASICEnabled:" << m_altirraBASICEnabled;
    qDebug() << "  m_basicRomPath:" << m_basicRomPath;

    // Configure OS ROM (Altirra or External)
    if (m_altirraOSEnabled) {
        qDebug() << "  -> Using Altirra OS (m_altirraOSEnabled = true)";
        // Use built-in Altirra OS for all machine types
        argList << "-xl-rev" << "altirra";
        qDebug() << "  -> Added arguments: -xl-rev altirra";
    } else {
        if (!m_osRomPath.isEmpty()) {
            qDebug() << "  Checking external OS ROM path...";
            QFileInfo osRomFile(m_osRomPath);
            bool romExists = osRomFile.exists();
            qDebug() << "  QFileInfo::exists() returned:" << romExists;
            qDebug() << "  Absolute path:" << osRomFile.absoluteFilePath();
            qDebug() << "  Is readable:" << osRomFile.isReadable();

            if (romExists) {
                qDebug() << "  -> Using external OS ROM";
                // CRITICAL: Must set -rev AUTO BEFORE the ROM file to reset any previously cached revision
                // This clears the Altirra OS setting and allows libatari800 to auto-select the custom ROM
                // NOTE: Must use uppercase "AUTO" - atari800 uses case-sensitive strcmp() for this value
                // NOTE: Do NOT use quotePath() - we're using char* array, not shell string
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "AUTO";
                    argList << "-5200_rom" << m_osRomPath;
                    qDebug() << "  -> Added arguments: -5200-rev AUTO -5200_rom" << m_osRomPath;
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "AUTO";
                    argList << "-osb_rom" << m_osRomPath;  // 800 OS-B ROM
                    qDebug() << "  -> Added arguments: -800-rev AUTO -osb_rom" << m_osRomPath;
                } else {
                    // For XL/XE machines
                    argList << "-xl-rev" << "AUTO";
                    argList << "-xlxe_rom" << m_osRomPath;
                    qDebug() << "  -> Added arguments: -xl-rev AUTO -xlxe_rom" << m_osRomPath;
                }
            } else {
                // File doesn't exist - fallback to Altirra OS
                qWarning() << "  -> OS ROM file not found, falling back to Altirra OS";
                qWarning() << "OS ROM file not found at" << m_osRomPath << "- Loading Altirra OS";
                if (machineType == "-5200") {
                    argList << "-5200-rev" << "altirra";
                    qDebug() << "  -> Added arguments: -5200-rev altirra";
                } else if (machineType == "-atari") {
                    argList << "-800-rev" << "altirra";
                    qDebug() << "  -> Added arguments: -800-rev altirra";
                } else {
                    argList << "-xl-rev" << "altirra";
                    qDebug() << "  -> Added arguments: -xl-rev altirra";
                }
            }
        } else {
            qDebug() << "  -> m_osRomPath is empty, falling back to Altirra OS";
            // Fallback to Altirra OS if no external ROM is specified
            if (machineType == "-5200") {
                argList << "-5200-rev" << "altirra";
                qDebug() << "  -> Added arguments: -5200-rev altirra";
            } else if (machineType == "-atari") {
                argList << "-800-rev" << "altirra";
                qDebug() << "  -> Added arguments: -800-rev altirra";
            } else {
                argList << "-xl-rev" << "altirra";
                qDebug() << "  -> Added arguments: -xl-rev altirra";
            }
        }
    }

    // Configure BASIC ROM according to user settings
    // This ensures OS disks (like SmartDOS + FujiNet) can access BASIC when needed
    if (m_altirraBASICEnabled) {
        // User explicitly wants Altirra BASIC
        argList << "-basic-rev" << "altirra";
        qDebug() << "  -> Configured Altirra BASIC ROM";
    } else if (!m_basicRomPath.isEmpty()) {
        // User specified an external BASIC ROM
        QFileInfo basicRomFile(m_basicRomPath);
        if (basicRomFile.exists()) {
            // CRITICAL: Must set -basic-rev AUTO BEFORE the ROM file to reset any previously cached revision
            // NOTE: Do NOT quote - we're using char* array, not shell string
            argList << "-basic-rev" << "AUTO";
            argList << "-basic_rom" << m_basicRomPath;
            qDebug() << "  -> Configured external BASIC ROM:" << m_basicRomPath;
        } else {
            qWarning() << "BASIC ROM file not found at" << m_basicRomPath;
            qDebug() << "  -> No BASIC ROM configured";
            // Don't configure any BASIC ROM - just use OS
        }
    } else {
        // No BASIC ROM preference specified - just use OS
        qDebug() << "  -> No BASIC ROM configured - OS only";
        // Don't add any BASIC ROM configuration
    }

    // Separately control whether to boot into BASIC mode
    // When NetSIO/FujiNet is enabled, use -nobasic so OS can boot properly
    if (basicEnabled) {
        argList << "-basic";  // Boot into BASIC prompt
        qDebug() << "  -> Boot mode: BASIC prompt";
    } else {
        argList << "-nobasic";  // Boot to OS/DOS (don't go to BASIC prompt)
        qDebug() << "  -> Boot mode: OS/DOS (no BASIC prompt) - Required for FujiNet/NetSIO";
    }
    
    // Add NetSIO support if enabled (use same port as FujiNet-PC so they match)
    if (netSIOEnabled) {
        QSettings netSettings("8bitrelics", "Fujisan");
        int netsioPort = netSettings.value("fujinet/netsioPort", 9997).toInt();
        argList << "-netsio" << QString::number(netsioPort);
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

    // Hard Drive (H:) device support via command-line arguments
    QSettings settings;
    bool anyHDriveEnabled = false;

    for (int i = 0; i < 4; i++) {
        QString hdKey = QString("media/hd%1").arg(i + 1);
        bool enabled = settings.value(hdKey + "Enabled", false).toBool();
        QString path = settings.value(hdKey + "Path", "").toString();

        if (enabled && !path.isEmpty()) {
            QFileInfo dirInfo(path);
            if (dirInfo.exists() && dirInfo.isDir()) {
                argList << QString("-H%1").arg(i + 1) << path;
                anyHDriveEnabled = true;
                qDebug() << QString("H%1: enabled with path:").arg(i + 1) << path;
            } else {
                qWarning() << QString("H%1: directory not accessible:").arg(i + 1) << path;
            }
        }
    }

    // Optional settings
    if (settings.value("media/hdReadOnly", false).toBool()) {
        argList << "-hreadonly";
        qDebug() << "H: read-only mode enabled";
    } else {
        argList << "-hreadwrite";
    }

    QString deviceName = settings.value("media/hdDeviceName", "H").toString();
    if (!deviceName.isEmpty() && deviceName != "H") {
        argList << "-Hdevicename" << deviceName;
        qDebug() << "H: device name set to:" << deviceName;
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

    // Log complete argument list being passed to libatari800
    qDebug() << "=== COMPLETE ARGUMENT LIST FOR libatari800_init() ===";
    QString argString;
    for (const QString& arg : argList) {
        argString += arg + " ";
    }
    qDebug() << "  Arguments:" << argString.trimmed();
    qDebug() << "  Total argument count:" << argList.size();

    // Force complete libatari800 reset to clear any persistent ROM state.
    // shutdown() already calls libatari800_exit() and clears m_libatari800Initialized,
    // so this guard only fires when initializeWithInputConfig() is called without a
    // prior shutdown() (e.g. the very first initialization path).
    if (m_libatari800Initialized) {
        qDebug() << "=== libatari800 RESTART DETECTED (no prior shutdown) ===";
        qDebug() << "  Calling libatari800_exit() to reset state...";
        libatari800_exit();
        m_libatari800Initialized = false;
        qDebug() << "  libatari800_exit() completed";
    } else {
        qDebug() << "=== FIRST libatari800 INITIALIZATION ===";
    }

    qDebug() << "  Calling libatari800_init()...";
    if (libatari800_init(argBytes.size(), args.data())) {
        qDebug() << "  libatari800_init() returned SUCCESS";
        m_libatari800Initialized = true;

        // H: device is CIO-based; re-enable its patch even when -netsio disabled it
        if (anyHDriveEnabled) {
            Devices_enable_h_patch = TRUE;
            Devices_UpdatePatches();
            qDebug() << "H: device patch re-enabled after libatari800_init";
        }

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

        // Initialize absolute-time frame scheduler and reset PI state
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_frameCount     = 0;
        m_piIntegral     = 0.0;
        m_piSpeedTrim    = 0.0;
        m_avgGap         = 0.0;

        // Schedule first frame
        requestNextFrame();
#ifdef HAVE_SDL2_JOYSTICK
        // shutdown() calls SDL2JoystickManager::shutdown() before lib exit; re-open SDL
        // and restore polling if real joysticks were enabled (restart path).
        if (m_joystickManager) {
            if (!m_joystickManager->initialize()) {
                qWarning() << "SDL joystick re-initialization after restart failed";
            } else {
                m_joystickManager->setEnabled(m_realJoysticksEnabled);
            }
        }
#endif
        qDebug() << "=== INITIALIZATION COMPLETE - Emulator started successfully ===";
        return true;
    }

    qDebug() << "  libatari800_init() returned FAILURE";
    qDebug() << "=== INITIALIZATION FAILED ===";
    return false;
}

bool AtariEmulator::initializeWithNetSIOConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                             const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                             const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                             bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks,
                                             bool netSIOEnabled, bool rtimeEnabled)
{
    qDebug() << "[NETSIO] initializeWithNetSIOConfig START";
    qDebug() << "[NETSIO] NetSIO requested:" << netSIOEnabled;
    qDebug() << "[NETSIO] Previous m_netSIOEnabled:" << m_netSIOEnabled;

    // Track NetSIO state for cold boot re-initialization
    m_netSIOEnabled = netSIOEnabled;
    qDebug() << "[NETSIO] Updated m_netSIOEnabled to:" << m_netSIOEnabled;

    // Track NetSIO state - no longer force BASIC disabled
    // SmartDOS/MyDOS will handle BASIC loading the same way as physical disks
    setBasicEnabled(basicEnabled);

    // Start with basic initialization first
    if (!initializeWithInputConfig(basicEnabled, machineType, videoSystem, artifactMode,
                                 horizontalArea, verticalArea, horizontalShift, verticalShift,
                                 fitScreen, show80Column, vSyncEnabled,
                                 kbdJoy0Enabled, kbdJoy1Enabled, swapJoysticks, netSIOEnabled)) {
        return false;
    }
    
    // Debug SIO patch status after initialization (for NetSIO investigation)
    debugSIOPatchStatus();
    
    // NetSIO is now initialized by atari800 core via -netsio command line argument
    if (netSIOEnabled) {
        qDebug() << "[NETSIO] Initialization in progress...";

        // Dismount all local disks to give FujiNet boot priority
        for (int i = 1; i <= 8; i++) {
            dismountDiskImage(i);
        }
        qDebug() << "[NETSIO] All local disks dismounted for FujiNet priority";

        // Send test command to verify NetSIO communication with FujiNet-PC
        // This is only needed for initial connection - subsequent boots use reset packets
#ifdef NETSIO
        extern void netsio_test_cmd(void);
        qDebug() << "[NETSIO] Sending test command for initial connection verification";
        qDebug() << "[NETSIO] netsio_enabled before test:" << netsio_enabled;
        netsio_test_cmd();
        qDebug() << "[NETSIO] netsio_enabled after test:" << netsio_enabled;
        if (!netsio_enabled) {
            // FujiNet-PC was not ready at boot time.  MainWindow::onFujiNetConnected()
            // will detect this flag and trigger an automatic cold boot once the HTTP
            // health check confirms FujiNet-PC is up.
            m_pendingFujiNetBoot = true;
            qDebug() << "[NETSIO] FujiNet not ready at init - pending cold boot set";
        } else {
            m_pendingFujiNetBoot = false;
        }
#else
        qDebug() << "[NETSIO] WARNING: Not compiled - test command not available";
#endif
    } else {
        qDebug() << "[NETSIO] Not requested - skipping initialization";
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
    m_shuttingDown.store(false);  // reset so restart works
    // Only stop the frame timer if we're on the thread that owns it.
    // When called after stopEmulatorWorkerIfRunning (worker already quit),
    // the timer is already inactive — skip to avoid the cross-thread warning.
    if (m_frameTimer && QThread::currentThread() == thread()) {
        m_frameTimer->stop();
    }

#ifdef HAVE_SDL2_JOYSTICK
    if (m_joystickManager) {
        m_joystickManager->shutdown();
    }
#endif

    if (!m_libatari800Initialized) {
        return;
    }

    m_libatari800Initialized = false;

    if (BINLOAD_bin_file != NULL) {
        fclose(BINLOAD_bin_file);
        BINLOAD_bin_file = NULL;
    }
    BINLOAD_start_binloading = FALSE;

#ifdef NETSIO
    netsio_shutdown();
#endif

    libatari800_exit();
}

void AtariEmulator::startDeferredTimers()
{
    if (!m_deferTimerStart) {
        return;
    }
    m_deferTimerStart = false;
    // Reset the absolute-time baseline so the first frame is scheduled from now.
    m_firstFrameTime = std::chrono::steady_clock::now();
    m_frameCount = 0;
    // Arm the frame timer on this (worker) thread.
    if (m_libatari800Initialized && !m_shuttingDown.load()) {
        m_frameTimer->start(0);
    }
#ifdef HAVE_SDL2_JOYSTICK
    // Re-apply joystick polling on the worker thread.
    if (m_joystickManager && m_realJoysticksEnabled) {
        m_joystickManager->setEnabled(true);
    }
#endif
}

void AtariEmulator::finalizeShutdownOnWorkerAndRehomeToGui()
{
    if (QThread::currentThread() != thread()) {
        return;
    }
    shutdown();
    teardownAudio();
    // Drop QTimer::singleShot / queued slot calls targeting this object on the worker.
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    QThread* gui = QCoreApplication::instance()->thread();
    if (thread() != gui) {
        moveToThread(gui);
    }
    QThread::currentThread()->quit();
}

void AtariEmulator::teardownAudio()
{
    if (m_audioOutput) {
        m_audioOutput->disconnect();
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
        m_audioDevice = nullptr;
    }
#ifdef HAVE_SDL2_AUDIO
    if (m_unifiedAudio) {
        delete m_unifiedAudio;
        m_unifiedAudio = nullptr;
    }
    if (m_sdl2Audio) {
        delete m_sdl2Audio;
        m_sdl2Audio = nullptr;
    }
#endif
}

void AtariEmulator::clearCurrentInputLocked()
{
    // libatari800_clear_input_array() also writes lib globals (e.g. INPUT_key_code).
    // Call it only on the emulator thread; UI/TCP call inject* from other threads and
    // must only update m_currentInput under the mutex — processFrame() passes the
    // snapshot into libatari800_next_frame on the correct thread.
    const bool onEmulatorThread = (QThread::currentThread() == thread());
    if (m_libatari800Initialized && onEmulatorThread) {
        libatari800_clear_input_array(&m_currentInput);
    } else {
        std::memset(&m_currentInput, 0, sizeof(m_currentInput));
    }
    m_currentInput.joy0 = 0x0f ^ 0xff;
    m_currentInput.joy1 = 0x0f ^ 0xff;
    m_currentInput.trig0 = 0;
    m_currentInput.trig1 = 0;
}

void AtariEmulator::processFrame()
{
    if (!m_libatari800Initialized || m_shuttingDown.load()) {
        return;
    }

    // Advance frame counter; next interval will be computed in requestNextFrame()
    // at the end of this function (absolute-time scheduling, no per-frame work needed here).
    // Delayed restart mechanism removed - no longer needed!
    // NetSIO reset notifications (0xFF/0xFE packets) provide instant state sync
    // with FujiNet-PC, eliminating the need for polling and delays.

    // Process device-specific joystick input and update m_currentInput under the
    // input mutex so the snapshot taken below is always consistent.
#ifdef HAVE_SDL2_JOYSTICK
    if (m_joystickManager) {
        QMutexLocker inputLock(&m_inputMutex);
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

    // Take a thread-safe snapshot of the current input state.  The snapshot
    // is copied here under the input mutex so that handleKeyPress/handleKeyRelease
    // running on the main thread cannot corrupt the struct while libatari800
    // is blocked inside netsio_recv_byte() (up to NETSIO_RECV_BYTE_TIMEOUT_SEC).
    //
    // Also snapshot inject counters here (same lock). injectCharacter() can run on
    // another thread after libatari800_next_frame() returns but before the block
    // below; using live m_injectKeyFramesRemaining then would decrement the *new*
    // injection on the same frame as a key-up, skipping a hold frame and letting
    // POKEY see repeated keys without an AKEY_NONE between them.
    input_template_t inputSnapshot;
    int injectHoldAtStart = 0;
    int injectPostAtStart = 0;
    {
        QMutexLocker inputLock(&m_inputMutex);
        inputSnapshot = m_currentInput;
        injectHoldAtStart = m_injectKeyFramesRemaining;
        injectPostAtStart = m_injectPostReleaseFrames;
    }

    // libatari800_next_frame() may rewrite inputSnapshot; post-release accounting must use
    // whether our template was keyboard-idle *before* the frame, or we never decrement
    // m_injectPostReleaseFrames (snapshot often stays non-zero after the call).
    const bool templateKeyboardIdleBeforeFrame = inputSnapshot.keychar == 0 &&
                                                 inputSnapshot.keycode == 0 &&
                                                 inputSnapshot.special == 0;

    // Determine if we should use partial frame execution for precise breakpoints
    m_usePartialFrameExecution = m_breakpointsEnabled && !m_breakpoints.isEmpty() && !m_emulationPaused;
    
    // For now, disable partial frame execution since it requires patches
    // We'll use frame-level execution with breakpoint checking after each frame
    // This is less precise but works with standard libatari800
    if (false && m_usePartialFrameExecution) {
        // Partial frame execution disabled - requires patches
    } else {
        // Normal full-frame execution; no lock held here — this call can block
        // for up to NETSIO_RECV_BYTE_TIMEOUT_SEC (3 s) when FujiNet is slow.
        libatari800_next_frame(&inputSnapshot);

        // Check breakpoints once after the frame (legacy behavior)
        if (m_breakpointsEnabled && !m_breakpoints.isEmpty()) {
            checkBreakpoints();
        }
    }

    {
        QMutexLocker inputLock(&m_inputMutex);
        if (injectHoldAtStart > 0) {
            m_injectKeyFramesRemaining--;
            if (m_injectKeyFramesRemaining == 0) {
                clearCurrentInputLocked();
                m_injectPostReleaseFrames = kInjectPostReleaseFrameCount;
            }
        } else if (injectPostAtStart > 0 && templateKeyboardIdleBeforeFrame) {
            if (m_injectPostReleaseFrames > 0) {
                m_injectPostReleaseFrames--;
            }
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
        // Audio diagnostics (enabled only when m_enableAudioDiagnostics is true)
        static bool s_audioDiagnosticsInitialized = false;
        static std::chrono::steady_clock::time_point s_lastFrameTime;
        static QFile s_audioDiagFile;

        if (m_enableAudioDiagnostics && !s_audioDiagnosticsInitialized) {
            s_audioDiagnosticsInitialized = true;

            // Open CSV log file in the user's writable location
            QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(logDir);
            QString logPath = logDir + QDir::separator() + "audio_diagnostics.csv";

            s_audioDiagFile.setFileName(logPath);
            if (s_audioDiagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QTextStream ts(&s_audioDiagFile);
                ts << "ms_since_start,frame_interval_ms,sound_buffer_len,dsp_gap_bytes,bytes_free,audio_state\n";
                ts.flush();
            } else {
                qWarning() << "Failed to open audio diagnostics log file at" << logPath;
            }

            s_lastFrameTime = std::chrono::steady_clock::now();
        } else if (!m_enableAudioDiagnostics && s_audioDiagnosticsInitialized) {
            // Diagnostics disabled at runtime
            s_audioDiagnosticsInitialized = false;
            if (s_audioDiagFile.isOpen()) {
                s_audioDiagFile.close();
            }
        }

        unsigned char* soundBuffer = libatari800_get_sound_buffer();
        int soundBufferLen = libatari800_get_sound_buffer_len();
        
        // DSP buffer must exist before touching the ring (never abort the whole frame)
        if (m_dspBufferBytes > 0 && !m_dspBuffer.isEmpty() && soundBuffer && soundBufferLen > 0) {
            // Write to DSP buffer (producer side)
            int gap = m_dspWritePos - m_dspReadPos;
            
            // Handle wrap-around
            if (gap < 0) {
                gap += m_dspBufferBytes;
            }
            
            // PI audio-clock feedback: gently adjusts m_frameTimeMs to keep the
            // DSP ring buffer at its target fill level.  Corrections are ±0.5% max,
            // so there are no audible pitch changes.
            if (m_userRequestedSpeedMultiplier != 0.0) {
                updateEmulationSpeed();
            }

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
                // Write sound data into the ring buffer.
                // On wrap we assemble through the staging buffer first so the
                // ring buffer is updated in a single contiguous operation.
                int writeOffset = m_dspWritePos % m_dspBufferBytes;
                int newPos = m_dspWritePos + soundBufferLen;
                if (writeOffset + soundBufferLen <= m_dspBufferBytes) {
                    // No wrap — direct copy
                    memcpy(m_dspBuffer.data() + writeOffset, soundBuffer, soundBufferLen);
                } else {
                    // Wrap: assemble contiguous copy through staging buffer then split into ring
                    int firstPartSize = m_dspBufferBytes - writeOffset;
                    memcpy(m_dspStagingBuffer.data(), soundBuffer, soundBufferLen);
                    memcpy(m_dspBuffer.data() + writeOffset,
                           m_dspStagingBuffer.data(), firstPartSize);
                    memcpy(m_dspBuffer.data(),
                           m_dspStagingBuffer.data() + firstPartSize,
                           soundBufferLen - firstPartSize);
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

            // Audio diagnostics logging
            if (m_enableAudioDiagnostics && s_audioDiagFile.isOpen()) {
                using namespace std::chrono;
                static steady_clock::time_point s_startTime = steady_clock::now();
                steady_clock::time_point now = steady_clock::now();

                double msSinceStart =
                    duration_cast<duration<double, std::milli>>(now - s_startTime).count();
                double frameIntervalMs =
                    duration_cast<duration<double, std::milli>>(now - s_lastFrameTime).count();
                s_lastFrameTime = now;

                int audioState = m_audioOutput->state();

                QTextStream ts(&s_audioDiagFile);
                ts << QString::number(msSinceStart, 'f', 3) << ","
                   << QString::number(frameIntervalMs, 'f', 3) << ","
                   << soundBufferLen << ","
                   << gap << ","
                   << bytesFree << ","
                   << audioState << "\n";
                ts.flush();
            }

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
                // Read from DSP buffer and write to audio device.
                // Always issue a single write() call so there is no window in which
                // the Qt audio pull thread could observe a partial update.
                // On wrap, assemble through the staging buffer first.
                int readOffset  = m_dspReadPos % m_dspBufferBytes;
                int newReadPos  = m_dspReadPos + toWrite;
                const char* writePtr;
                if (readOffset + toWrite <= m_dspBufferBytes) {
                    // No wrap — write directly from ring buffer
                    writePtr = m_dspBuffer.data() + readOffset;
                } else {
                    // Wrap — assemble contiguous chunk in staging buffer (may exceed one
                    // emulator frame; size is bounded by Qt bytesFree(), e.g. 8192 bytes).
                    if (toWrite > m_dspStagingBuffer.size()) {
                        m_dspStagingBuffer.resize(toWrite);
                    }
                    int firstPartSize = m_dspBufferBytes - readOffset;
                    memcpy(m_dspStagingBuffer.data(),
                           m_dspBuffer.data() + readOffset, firstPartSize);
                    memcpy(m_dspStagingBuffer.data() + firstPartSize,
                           m_dspBuffer.data(), toWrite - firstPartSize);
                    writePtr = m_dspStagingBuffer.data();
                }
                m_audioDevice->write(writePtr, toWrite);
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

    emit frameReady(renderFrameImage());

    // Schedule the next frame if emulation is running
    if (!m_emulationPaused) {
        requestNextFrame();
    }
}

QImage AtariEmulator::renderFrameImage()
{
    // Screen dimensions match libatari800's Screen_WIDTH / Screen_HEIGHT (384 x 240).
    static const int W = 384;
    static const int H = 240;

    if (m_frameImage.isNull()) {
        m_frameImage = QImage(W, H, QImage::Format_RGB32);
    }

    const unsigned char* screen = libatari800_get_screen_ptr();
    if (!screen) {
        return m_frameImage;
    }

    for (int y = 0; y < H; y++) {
        QRgb* scanLine = reinterpret_cast<QRgb*>(m_frameImage.scanLine(y));
        for (int x = 0; x < W; x++) {
            unsigned char colorIndex = screen[y * W + x];
            int rgb = Colours_table[colorIndex];
            scanLine[x] = qRgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        }
    }

    // Return by value. Qt's copy-on-write ensures the data is detached when
    // this function is called again for the next frame while the previous
    // image is still referenced by the queued signal on the main thread.
    return m_frameImage;
}

void AtariEmulator::requestNextFrame()
{
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_deferTimerStart) {
        // Timer start is deferred until startDeferredTimers() is called on the worker.
        return;
    }
    // Absolute-time scheduling: each frame fires at firstFrameTime + frameCount * frameTimeMs.
    // Any Qt timer overshoot or undershoot is automatically corrected on the next frame
    // because we always measure the delay relative to the fixed absolute origin.
    using namespace std::chrono;

    // Unlimited speed: fire as fast as possible, bypass all timing logic.
    if (m_userRequestedSpeedMultiplier == 0.0) {
        m_frameCount++;
        m_frameTimer->start(0);
        return;
    }

    m_frameCount++;

    auto nextFrameTime = m_firstFrameTime +
        duration<double, std::milli>(m_frameCount * static_cast<double>(m_frameTimeMs));

    auto now = steady_clock::now();
    auto delayUs = duration_cast<microseconds>(nextFrameTime - now).count();

    // Debt cap (mirrors standalone atari800's Atari800_Sync self-healing):
    // If we've fallen more than one full frame behind wall-clock time, discard
    // the accumulated debt by resetting the time baseline.  Without this,
    // netsio_wait_for_sync() blocking inside ANTIC_Frame() causes an ever-growing
    // catch-up backlog where each intervalMs=0 frame that hits another SIO command
    // adds more blocking, creating a feedback loop of progressive slowdown.
    const auto frameTimeUs = static_cast<int64_t>(m_frameTimeMs * 1000.0);
    if (delayUs < -frameTimeUs) {
        m_firstFrameTime = now;
        // Next line in this function is m_frameCount++, so use 0 here so the first
        // deadline after reset is firstFrameTime + 1 * frameTime (not +2).
        m_frameCount = 0;
        delayUs = frameTimeUs;
    }

    // Convert microseconds to milliseconds, clamping to [0, frameTimeMs] range
    int intervalMs = 0;
    if (delayUs > 0) {
        // Round to nearest ms; clamp so we never schedule more than one full frame ahead
        intervalMs = static_cast<int>((delayUs + 500) / 1000);
        int maxInterval = static_cast<int>(m_frameTimeMs) + 1;
        if (intervalMs > maxInterval) intervalMs = maxInterval;
    }

    m_frameTimer->start(intervalMs);
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
        // Load XEX/EXE/COM files as executables using BINLOAD.
        // BINLOAD_Loader() calls Atari800_Coldstart() internally, which sends a 0xFF
        // cold-reset packet to FujiNet-PC via netsio_cold_reset(), causing it to
        // exit(75) and restart.  Suppress the packet the same way coldBoot() does so
        // that loading a XEX does not kill the FujiNet connection.
#ifdef NETSIO
        if (m_netSIOEnabled && netsio_enabled) {
            int savedNetsioEnabled = netsio_enabled;
            netsio_enabled = 0;
            int result = BINLOAD_Loader(filename.toUtf8().constData());
            netsio_enabled = savedNetsioEnabled;
            return result != 0;
        }
#endif
        return BINLOAD_Loader(filename.toUtf8().constData()) != 0;
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

    m_firstFrameTime = std::chrono::steady_clock::now();
    m_frameCount = 0;
    if (m_emulationPaused) {
        m_emulationPaused = false;
        requestNextFrame();
    }
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
    QMutexLocker inputLock(&m_inputMutex);
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
    bool altPressed = modifiers & Qt::AltModifier;

    // On macOS, support both Ctrl and Cmd keys for control codes
    bool controlKeyPressed = ctrlPressed || metaPressed;
    
    // Process key input
    
    // Handle Control key combinations using proper AKEY codes  
    // Build modifier bits like atari800 SDL does
    if (controlKeyPressed && key >= Qt::Key_A && key <= Qt::Key_Z) {
        unsigned char baseKey = convertQtKeyToAtari(key, Qt::NoModifier);
        // Build shiftctrl modifier bits
        // Note: Even if baseKey is 0 (like AKEY_l), the final keycode will be non-zero
        // because of the modifier bits (AKEY_CTRL = 0x80)
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
    } else if (key == Qt::Key_CapsLock) {
        // Send CAPS LOCK toggle to the emulator to change its internal state
        m_currentInput.keycode = AKEY_CAPSTOGGLE;
        // Update our internal caps lock state tracking
        m_capsLockEnabled = !m_capsLockEnabled;
    } else if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        if (key == Qt::Key_L) {
            // Special handling for L key since AKEY_l = 0 causes issues with the keycode path.
            // Use keychar so the core maps it through the character table, which handles case via CAPS LOCK.
            // When Shift is held, send uppercase 'L' so the core maps it to AKEY_L (AKEY_SHFT | AKEY_l).
            m_currentInput.keychar = shiftPressed ? 'L' : 'l';
        } else {
            // Normal letter handling using keycode; OR in AKEY_SHFT when Shift is held so the
            // emulator receives an uppercase key regardless of the current CAPS LOCK state.
            unsigned char baseKey = convertQtKeyToAtari(key, Qt::NoModifier);
            m_currentInput.keycode = shiftPressed ? (baseKey | AKEY_SHFT) : baseKey;
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
    } else if (key == Qt::Key_F5) {
        if (altPressed) {
            // Option/Alt+F5 = Insert character; Shift+Option/Alt+F5 = Insert line (macOS mapping)
            if (shiftPressed) {
                m_currentInput.keycode = AKEY_INSERT_LINE;
            } else {
                m_currentInput.keycode = AKEY_INSERT_CHAR;
            }
        } else if (shiftPressed) {
            // Shift+F5 = Cold Reset (Power) - libatari800 does: lastkey = -input->special
            m_currentInput.special = -AKEY_COLDSTART;  // Convert -3 to +3
        } else {
            // F5 = Warm Reset - libatari800 does: lastkey = -input->special
            m_currentInput.special = -AKEY_WARMSTART;  // Convert -2 to +2
        }
    } else if (key == Qt::Key_F6) {
        if (altPressed) {
            // Option/Alt+F6 = Delete character; Shift+Option/Alt+F6 = Delete line (macOS mapping)
            if (shiftPressed) {
                m_currentInput.keycode = AKEY_DELETE_LINE;
            } else {
                m_currentInput.keycode = AKEY_DELETE_CHAR;
            }
        } else {
            // F6 = Help
            m_currentInput.keycode = AKEY_HELP;
        }
    } else if (key == Qt::Key_F7 || key == Qt::Key_Pause) {
        if (altPressed && key == Qt::Key_F7) {
            // Option/Alt+F7 = Clear screen (macOS mapping)
            m_currentInput.keycode = AKEY_CLEAR;
        } else {
            // F7 or Pause = Break - libatari800 does: lastkey = -input->special
            m_currentInput.special = -AKEY_BREAK;  // Convert -5 to +5
        }
    } else if (key == Qt::Key_F8) {
        // F8 = Clear
        m_currentInput.keycode = AKEY_CLEAR;
    } else if (key == Qt::Key_Insert) {
        // Insert = Insert character, Shift+Insert = Insert line
        if (shiftPressed) {
            m_currentInput.keycode = AKEY_INSERT_LINE;
        } else {
            m_currentInput.keycode = AKEY_INSERT_CHAR;
        }
    } else if (key == Qt::Key_Delete) {
        // Delete = Delete character, Shift+Delete = Delete line
        if (shiftPressed) {
            m_currentInput.keycode = AKEY_DELETE_LINE;
        } else {
            m_currentInput.keycode = AKEY_DELETE_CHAR;
        }
#ifdef Q_OS_MACOS
    } else if (key == Qt::Key_Backspace && shiftPressed) {
        // On macOS, the physical "delete" key sends Backspace, not Delete
        // Shift+Delete (Backspace) = Delete line
        m_currentInput.keycode = AKEY_DELETE_LINE;
#endif
    } else if (key == Qt::Key_Home && shiftPressed) {
        // Shift+Home = Clear screen
        m_currentInput.keycode = AKEY_CLEAR;
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
    QMutexLocker inputLock(&m_inputMutex);
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
    qDebug() << "[NETSIO] COLD BOOT START — m_netSIOEnabled:" << m_netSIOEnabled;
#ifdef NETSIO
    qDebug() << "[NETSIO] netsio_enabled:" << netsio_enabled;
    // If FujiNet mode is active but the DEVICE_CONNECTED handshake was missed
    // (e.g. FujiNet-PC was already running when Fujisan started), force-enable
    // netsio so the cold boot suppresses the 0xFF packet and the Atari can boot
    // from FujiNet after the local disks are dismounted below.
    ensureNetsioEnabled();
#endif

    // Reset the Atari.  When NetSIO is active we suppress the 0xFF cold-reset packet
    // that Atari800_Coldstart() would normally send: that packet causes FujiNet-PC to
    // call exit(75) and restart, which would happen right as the Atari tries to boot
    // from it.  FujiNet-PC restart is now handled explicitly by the FujiNet Reset
    // button in the sidebar — the emulator's cold boot no longer owns FujiNet's lifecycle.
#ifdef NETSIO
    if (m_netSIOEnabled && netsio_enabled) {
        int savedNetsioEnabled = netsio_enabled;
        netsio_enabled = 0;
        qDebug() << "Calling Atari800_Coldstart() (internal 0xFF suppressed)...";
        Atari800_Coldstart();
        netsio_enabled = savedNetsioEnabled;
        qDebug() << "Atari800_Coldstart() completed";
    } else {
        qDebug() << "Calling Atari800_Coldstart()...";
        Atari800_Coldstart();
        qDebug() << "Atari800_Coldstart() completed";
    }
#else
    qDebug() << "Calling Atari800_Coldstart()...";
    Atari800_Coldstart();
    qDebug() << "Atari800_Coldstart() completed";
#endif

    // Dismount local disks to give FujiNet boot priority
    if (m_netSIOEnabled) {
        qDebug() << "Dismounting local disks for FujiNet priority";
        for (int i = 1; i <= 8; i++) {
            dismountDiskImage(i);
        }
        qDebug() << "Local disk dismounting complete";
    }

    qDebug() << "=== COLD BOOT COMPLETE ===";
#ifdef NETSIO
    // Clear the pending-boot flag regardless of how we got here.
    m_pendingFujiNetBoot = false;
#endif

    // Reset the absolute-time frame scheduler baseline so the timing debt that
    // accumulated while waiting for the cold boot to be delivered does not cause
    // a burst of catch-up frames immediately after the reset.
    m_firstFrameTime = std::chrono::steady_clock::now();
    m_frameCount = 0;

    // If emulation was paused when cold boot was pressed, restart the frame loop
    // so the reset screen is actually rendered and displayed.
    if (m_emulationPaused) {
        m_emulationPaused = false;
        requestNextFrame();
    }
}

void AtariEmulator::warmBoot()
{
    qDebug() << "[NETSIO] WARM BOOT START";
    qDebug() << "[NETSIO] m_netSIOEnabled:" << m_netSIOEnabled;

    // CRITICAL: Send warm reset notification to FujiNet-PC BEFORE resetting emulator
    // This tells FujiNet-PC to reset its state for warm boot
#ifdef NETSIO
    qDebug() << "[NETSIO] netsio_enabled (core variable):" << netsio_enabled;
    if (m_netSIOEnabled && netsio_enabled) {
        qDebug() << "[NETSIO] Sending warm reset to FujiNet-PC (0xFE packet)";
        int resetResult = netsio_warm_reset();
        qDebug() << "[NETSIO] netsio_warm_reset() returned:" << resetResult;
    } else {
        if (!m_netSIOEnabled) {
            qDebug() << "[NETSIO] WARNING: m_netSIOEnabled is FALSE - NetSIO disabled in Fujisan";
        }
        if (!netsio_enabled) {
            qDebug() << "[NETSIO] WARNING: netsio_enabled is FALSE - NetSIO not enabled in atari800 core";
        }
    }
#endif

    // Reset the Atari
    Atari800_Warmstart();
    qDebug() << "[NETSIO] WARM BOOT COMPLETE";

    // Reset the absolute-time frame scheduler baseline and restart the frame
    // loop if emulation was paused, so the reset screen is rendered.
    m_firstFrameTime = std::chrono::steady_clock::now();
    m_frameCount = 0;
    if (m_emulationPaused) {
        m_emulationPaused = false;
        requestNextFrame();
    }
}

void AtariEmulator::resetNetSIOClientState()
{
#ifdef NETSIO
    extern int fujinet_known;
    // Clear netsio client state when FujiNet-PC is stopped externally (e.g. via TCP command).
    // This prevents the emulator from routing SIO to the dead process, and allows the new
    // FujiNet-PC process to perform a clean handshake from scratch when it restarts.
    netsio_enabled = 0;
    fujinet_known = 0;
    netsio_sync_wait = 0;
    netsio_cmd_state = 0;
    // Drain FujiNet->emulator RX: Windows uses netsiowin ring buffer (library symbol);
    // POSIX uses the pipe in netsio.c — drain here so we do not require netsio_flush_fifo
    // in libatari800 (upstream atari800 may not export it).
#ifdef _WIN32
    netsio_flush_fifo();
#else
    {
        extern int fds0[2];
        if (fds0[0] >= 0) {
            int flags = fcntl(fds0[0], F_GETFL, 0);
            fcntl(fds0[0], F_SETFL, flags | O_NONBLOCK);
            unsigned char discard[256];
            while (read(fds0[0], discard, sizeof(discard)) > 0)
                ;
            fcntl(fds0[0], F_SETFL, flags);
        }
    }
#endif
    qDebug() << "[NETSIO] Client state reset: netsio_enabled=0, fujinet_known=0, sync_wait=0, fifo flushed";
#endif
}

void AtariEmulator::ensureNetsioEnabled()
{
#ifdef NETSIO
    if (m_netSIOEnabled && !netsio_enabled) {
        qDebug() << "[NETSIO] ensureNetsioEnabled: forcing netsio_enabled=1"
                 << "(FujiNet mode active but DEVICE_CONNECTED handshake was missed)";
        netsio_enabled = 1;
    }
#endif
}

bool AtariEmulator::shouldAutoColdBootForFujiNet()
{
#ifdef NETSIO
    extern int fujinet_known;

    const bool netsioActive = m_netSIOEnabled && netsio_enabled && fujinet_known;
    if (m_pendingFujiNetBoot && netsioActive) {
        qDebug() << "[NETSIO] Clearing pending auto cold boot - NetSIO is already active";
        m_pendingFujiNetBoot = false;
    }
#endif

    return m_pendingFujiNetBoot;
}

bool AtariEmulator::updateHardDrivePath(int driveNumber, const QString& path)
{
    if (driveNumber < 1 || driveNumber > 4) {
        qWarning() << "updateHardDrivePath: invalid drive number" << driveNumber;
        return false;
    }

    int idx = driveNumber - 1;
    QByteArray pathBytes = path.toUtf8();

    if (pathBytes.size() >= FILENAME_MAX) {
        qWarning() << "updateHardDrivePath: path too long";
        return false;
    }

    strncpy(Devices_atari_h_dir[idx], pathBytes.constData(), FILENAME_MAX - 1);
    Devices_atari_h_dir[idx][FILENAME_MAX - 1] = '\0';

    if (!Devices_enable_h_patch) {
        Devices_enable_h_patch = TRUE;
        Devices_UpdatePatches();
        qDebug() << "H: device patch enabled on the fly";
    }

    qDebug() << QString("H%1: path updated at runtime to:").arg(driveNumber) << path;
    coldBoot();
    return true;
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
        case Qt::Key_Insert: return AKEY_INSERT_CHAR;
        case Qt::Key_Delete: return AKEY_DELETE_CHAR;
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
#if defined(_WIN32) || defined(__linux__)
    // Windows/Linux: Use larger fragments for stability with dynamic frame timing
    m_fragmentSize = 1024; // Larger fragments for better Qt audio stability (~23ms at 44100Hz)
    int targetDelayMs = 50;   // Extra headroom for UI operations and typing resilience
#else
    // macOS: Smaller buffers work fine here
    m_fragmentSize = 512; // Smaller fragments for more responsive audio
    int targetDelayMs = 40;  // Original delay for macOS
#endif
    int fragmentBytes = m_fragmentSize * m_bytesPerSample;
    
    // Set up DSP buffer to handle the rate mismatch
    // We generate 1472 bytes/frame but Qt consumes ~940 bytes/call
    // This means we accumulate ~532 bytes per frame (36% excess)
    // Buffer needs to be large enough to absorb this while we wait for consumption
    m_targetDelay = (m_sampleRate * targetDelayMs) / 1000;  // Convert to samples
    // Use a larger buffer to handle the accumulation
#if defined(_WIN32) || defined(__linux__)
    int dspBufferSamples = m_fragmentSize * 25;  // Extra-large buffer for maximum stability during typing/UI ops
#else
    int dspBufferSamples = m_fragmentSize * 10;  // Standard buffer for macOS
#endif
    m_dspBufferBytes = dspBufferSamples * m_bytesPerSample;
    
    // Initialize the DSP buffer and staging buffer
    m_dspBuffer.resize(m_dspBufferBytes);
    m_dspBuffer.fill(0);
    m_dspStagingBuffer.resize(DSP_STAGING_BYTES);
    m_dspStagingBuffer.fill(0);
    
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
    
    
    // Stop and release any previous QAudioOutput before creating a new one.
    // The tricky part: QAudioOutput registers internal timers on whichever thread
    // called start().  At startup that is the main thread (before moveToThread);
    // on subsequent restarts it is the emulator thread.  Calling stop() or the
    // destructor from the wrong thread triggers "Timers cannot be stopped from
    // another thread" and a crash.
    //
    // Strategy: disconnect all signals immediately so no callbacks fire, then
    // move the object to the main thread and let it self-destruct there via
    // deleteLater(), which guarantees timer teardown on the right thread.
    if (m_audioOutput) {
        // Disconnect first — stop() fires stateChanged which would call killTimer
        // on the wrong thread if the timer was registered on the main thread.
        m_audioOutput->disconnect();
        // stop() on the current (emulator) thread is safe now: no timer callbacks.
        m_audioOutput->stop();
        m_audioOutput->setParent(nullptr);  // detach so moveToThread is allowed
        m_audioOutput->moveToThread(QCoreApplication::instance()->thread());
        m_audioOutput->deleteLater();
        m_audioOutput = nullptr;
        m_audioDevice = nullptr;
    }

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
#if defined(_WIN32) || defined(__linux__)
    // Windows/Linux: Use music category for higher latency/buffering and stability
    m_audioOutput->setCategory("music");
#else
    // macOS: Use game category for lower latency (works fine with smaller buffers)
    m_audioOutput->setCategory("game");
#endif
    
    // Set volume to ensure audio is active
    m_audioOutput->setVolume(1.0);
    
    
    // Connect notify signal for audio callback timing
    connect(m_audioOutput, &QAudioOutput::notify, this, [this]() {
        // Update callback tick for gap estimation
        m_callbackTick = QDateTime::currentMSecsSinceEpoch();
    });

    // Underrun detection: in push mode, QAudioOutput enters IdleState when its
    // internal buffer drains.  We log the event and schedule a deferred recovery
    // via QMetaObject::invokeMethod so we never call write()/reset state from
    // directly inside the audio backend's signal emission path.
    connect(m_audioOutput, &QAudioOutput::stateChanged, this, [this](QAudio::State state) {
        if (state != QAudio::IdleState)
            return;
        if (!m_audioOutput || m_audioOutput->error() != QAudio::NoError)
            return;

        static int underrunCount = 0;
        underrunCount++;
        if (underrunCount <= 5 || underrunCount % 50 == 0) {
            qDebug() << "[Audio] Underrun #" << underrunCount
                     << "- scheduling deferred recovery";
        }

        // Defer all recovery work to the next event-loop iteration so we are
        // not executing inside the audio backend's signal emission stack.
        QMetaObject::invokeMethod(this, [this]() {
            if (!m_audioOutput || !m_audioDevice || !m_audioEnabled)
                return;
            // Only act if still in IdleState; ActiveState means recovery happened already
            if (m_audioOutput->state() != QAudio::IdleState)
                return;

            // Reset DSP ring buffer so the next processFrame() write starts clean
            m_dspWritePos = 0;
            m_dspReadPos  = 0;
            m_dspBuffer.fill(0);

            // Reset PI controller — stale integral is misleading after an underrun
            m_piIntegral  = 0.0;
            m_piSpeedTrim = 0.0;
            m_avgGap      = 0.0;

#ifdef __linux__
            // On Linux/PulseAudio, repeated underruns suggest bytesFree() is
            // under-reporting.  Grow the target delay by one fragment size.
            static int linuxUnderrunCount = 0;
            if (++linuxUnderrunCount % 5 == 0) {
                m_targetDelay = qMin(m_targetDelay + m_fragmentSize,
                                     m_dspBufferBytes / m_bytesPerSample / 2);
                qDebug() << "[Audio] Linux: increased target delay to"
                         << m_targetDelay << "samples after repeated underruns";
            }
#endif

            // Write a silence cushion so the device returns to ActiveState.
            // In push mode, writing data is sufficient — do NOT call resume()
            // which is only valid after suspend() and will crash on IdleState.
            int silenceBytes = qMin(m_targetDelay * m_bytesPerSample,
                                    m_audioOutput->bytesFree());
            if (silenceBytes > 0) {
                QByteArray silence(silenceBytes, 0);
                m_audioDevice->write(silence);
            }
        }, Qt::QueuedConnection);
    });

    m_audioDevice = m_audioOutput->start();

    if (m_audioDevice) {
        // Test what we actually get from libatari800
        int testBufferLen = libatari800_get_sound_buffer_len();
        Q_UNUSED(testBufferLen);
    } else {
        m_audioEnabled = false;
    }
    }  // End of QtAudio backend setup
}

void AtariEmulator::enableAudio(bool enabled)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enabled]() { enableAudio(enabled); }, Qt::QueuedConnection);
        return;
    }
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
                m_audioOutput->disconnect();
                m_audioOutput->stop();
                m_audioOutput->setParent(nullptr);
                m_audioOutput->moveToThread(QCoreApplication::instance()->thread());
                m_audioOutput->deleteLater();
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
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, volume]() { setVolume(volume); }, Qt::QueuedConnection);
        return;
    }
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
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, backend]() { setAudioBackend(backend); }, Qt::QueuedConnection);
        return;
    }
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

void AtariEmulator::setJoystick0Preset(const QString& preset)
{
    if (preset == "numpad" || preset == "arrows" || preset == "wasd") {
        m_joystick0Preset = preset;
    }
}

void AtariEmulator::setJoystick1Preset(const QString& preset)
{
    if (preset == "numpad" || preset == "arrows" || preset == "wasd") {
        m_joystick1Preset = preset;
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

    // Reset the absolute-time baseline so requestNextFrame() starts fresh from now.
    // This prevents debt accumulation when switching modes (e.g. from MAX back to 1x,
    // where wall-clock time has raced far ahead of the old baseline).
    m_firstFrameTime = std::chrono::steady_clock::now();
    m_frameCount = 0;

    // Recalculate m_frameTimeMs for the new speed so requestNextFrame() uses the
    // correct target interval immediately (unlimited uses nominal FPS for reference only).
    if (m_userRequestedSpeedMultiplier != 0.0) {
        double baseFps = static_cast<double>(m_targetFps > 0 ? m_targetFps : 50);
        m_frameTimeMs = static_cast<float>((1000.0 / baseFps) / m_userRequestedSpeedMultiplier);
    }

    qDebug() << "Speed set to" << percentage << "% (multiplier:" << m_userRequestedSpeedMultiplier
             << ") - Atari800_turbo:" << Atari800_turbo
             << "Atari800_turbo_speed:" << Atari800_turbo_speed
             << "frameTimeMs:" << m_frameTimeMs << "ms";
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
    // libatari800_reboot_with_file() calls Atari800_Coldstart() which sends a 0xFF
    // cold-reset packet to FujiNet-PC.  Suppress it so FujiNet stays connected.
#ifdef NETSIO
    int savedNetsioEnabled = 0;
    if (m_netSIOEnabled && netsio_enabled) {
        savedNetsioEnabled = netsio_enabled;
        netsio_enabled = 0;
    }
#endif

    if (!libatari800_reboot_with_file(filename.toUtf8().constData())) {
#ifdef NETSIO
        if (savedNetsioEnabled) netsio_enabled = savedNetsioEnabled;
#endif
        return false;
    }

#ifdef NETSIO
    if (savedNetsioEnabled) netsio_enabled = savedNetsioEnabled;
#endif
    
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
    QMutexLocker inputLock(&m_inputMutex);
    clearCurrentInputLocked();
    m_injectPostReleaseFrames = 0;

    if (ch >= 'A' && ch <= 'Z') {
        // Uppercase letters - use AKEY_A through AKEY_Z (includes SHIFT modifier)
        unsigned char atariKey = 0;
        switch (ch) {
            case 'A': atariKey = AKEY_A; break;
            case 'B': atariKey = AKEY_B; break;
            case 'C': atariKey = AKEY_C; break;
            case 'D': atariKey = AKEY_D; break;
            case 'E': atariKey = AKEY_E; break;
            case 'F': atariKey = AKEY_F; break;
            case 'G': atariKey = AKEY_G; break;
            case 'H': atariKey = AKEY_H; break;
            case 'I': atariKey = AKEY_I; break;
            case 'J': atariKey = AKEY_J; break;
            case 'K': atariKey = AKEY_K; break;
            case 'L': atariKey = AKEY_L; break;
            case 'M': atariKey = AKEY_M; break;
            case 'N': atariKey = AKEY_N; break;
            case 'O': atariKey = AKEY_O; break;
            case 'P': atariKey = AKEY_P; break;
            case 'Q': atariKey = AKEY_Q; break;
            case 'R': atariKey = AKEY_R; break;
            case 'S': atariKey = AKEY_S; break;
            case 'T': atariKey = AKEY_T; break;
            case 'U': atariKey = AKEY_U; break;
            case 'V': atariKey = AKEY_V; break;
            case 'W': atariKey = AKEY_W; break;
            case 'X': atariKey = AKEY_X; break;
            case 'Y': atariKey = AKEY_Y; break;
            case 'Z': atariKey = AKEY_Z; break;
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
    } else if (ch == '_') {
        // Underscore
        m_currentInput.keycode = AKEY_UNDERSCORE;
    } else if (ch == '$') {
        // Dollar sign
        m_currentInput.keycode = AKEY_DOLLAR;
    } else if (ch == '@') {
        // At sign
        m_currentInput.keycode = AKEY_AT;
    } else if (ch == '#') {
        // Hash
        m_currentInput.keycode = AKEY_HASH;
    } else if (ch == '%') {
        // Percent
        m_currentInput.keycode = AKEY_PERCENT;
    } else if (ch == '&') {
        // Ampersand
        m_currentInput.keycode = AKEY_AMPERSAND;
    } else if (ch == '[') {
        // Left bracket
        m_currentInput.keycode = AKEY_BRACKETLEFT;
    } else if (ch == ']') {
        // Right bracket
        m_currentInput.keycode = AKEY_BRACKETRIGHT;
    } else if (ch == '\\') {
        // Backslash
        m_currentInput.keycode = AKEY_BACKSLASH;
    } else if (ch == '|') {
        // Pipe
        m_currentInput.keycode = AKEY_BAR;
    } else if (ch == '^') {
        // Circumflex
        m_currentInput.keycode = AKEY_CIRCUMFLEX;
    } else {
        m_injectKeyFramesRemaining = 0;
        return;
    }

    // Keep the key asserted for several emulator-thread frames only — never call
    // libatari800_next_frame() from here (main thread / TCP thread); it races processFrame().
    m_injectKeyFramesRemaining = kInjectKeyHoldFrameCount;
}

void AtariEmulator::injectAKey(int akeyCode)
{
    {
        QMutexLocker inputLock(&m_inputMutex);
        clearCurrentInputLocked();

        // Directly set the raw AKEY code
        m_currentInput.keycode = akeyCode;
    }
    
    // Schedule key release after a short delay (one frame).
    // The lambda fires on the emulator thread's event loop (between frames),
    // so it also needs the mutex to guard against concurrent main-thread writes.
    QTimer::singleShot(50, this, [this]() {
        QMutexLocker inputLock(&m_inputMutex);
        clearCurrentInputLocked();
    });
}

int AtariEmulator::injectedKeyFramesRemainingForTest() const
{
    QMutexLocker inputLock(&m_inputMutex);
    return m_injectKeyFramesRemaining;
}

bool AtariEmulator::injectionTimersIdleForTest() const
{
    QMutexLocker inputLock(&m_inputMutex);
    return m_injectKeyFramesRemaining == 0 && m_injectPostReleaseFrames == 0;
}

bool AtariEmulator::isCharacterInjectionIdle() const
{
    QMutexLocker inputLock(&m_inputMutex);
    if (m_injectKeyFramesRemaining != 0 || m_injectPostReleaseFrames != 0)
        return false;
    // Also wait until the Atari OS has consumed the previous keystroke.
    // CH ($02FC = 764) holds 0xFF when no key is pending in the OS buffer.
    // Sending the next character before the OS clears CH causes a silent drop.
    if (m_libatari800Initialized) {
        unsigned char *mem = reinterpret_cast<unsigned char *>(libatari800_get_main_memory_ptr());
        if (mem && mem[764] != 0xFF)
            return false;
    }
    return true;
}

void AtariEmulator::clearInput()
{
    QMutexLocker inputLock(&m_inputMutex);
    clearCurrentInputLocked();
}

void AtariEmulator::setCapsLock(bool enabled)
{
    // Only toggle if the current state differs from desired state
    if (m_capsLockEnabled != enabled) {
        injectAKey(AKEY_CAPSTOGGLE);
        m_capsLockEnabled = enabled;
    }
}

bool AtariEmulator::getCapsLockState() const
{
    return m_capsLockEnabled;
}

bool AtariEmulator::isConfigDriveSlotsScreen() const
{
    // Check if FujiNet CONFIG's "DRIVE SLOTS" screen is displayed
    // by searching screen memory for the text "DRIVE SLOTS"

    // Get pointer to emulator memory
    unsigned char* mem = libatari800_get_main_memory_ptr();
    if (!mem) {
        return false;
    }

    // Internal screen codes for "DRIVE SLOTS":
    // D=36(0x24), R=50(0x32), I=41(0x29), V=54(0x36), E=37(0x25), space=0(0x00)
    // S=51(0x33), L=44(0x2C), O=47(0x2F), T=52(0x34), S=51(0x33)
    static const unsigned char driveSlots[] = {
        0x24, 0x32, 0x29, 0x36, 0x25, 0x00,  // "DRIVE "
        0x33, 0x2C, 0x2F, 0x34, 0x33         // "SLOTS"
    };
    static const int patternLen = sizeof(driveSlots);

    // Search all of memory for "DRIVE SLOTS" text
    // This is simpler and more reliable than parsing display lists
    for (int addr = 0; addr < 0x10000 - patternLen; addr++) {
        bool match = true;
        for (int j = 0; j < patternLen; j++) {
            // Mask off inverse video bit (bit 7) for comparison
            if ((mem[addr + j] & 0x7F) != driveSlots[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }

    return false;
}

void AtariEmulator::pauseEmulation()
{
    // Guard against cross-thread calls: m_frameTimer must be stopped from its
    // own thread.  If we're on the wrong thread, re-invoke via the event loop.
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "pauseEmulation", Qt::QueuedConnection);
        return;
    }
    if (!m_emulationPaused) {
        m_frameTimer->stop();
        {
            QMutexLocker inputLock(&m_inputMutex);
            clearCurrentInputLocked();
            m_injectKeyFramesRemaining = 0;
            m_injectPostReleaseFrames = 0;
        }
        m_emulationPaused = true;
        emit executionPaused();
    }
}

void AtariEmulator::resumeEmulation()
{
    // Guard against cross-thread calls: m_frameTimer must be started from its
    // own thread.  If we're on the wrong thread, re-invoke via the event loop.
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "resumeEmulation", Qt::QueuedConnection);
        return;
    }
    if (m_emulationPaused) {
        // Reset absolute-time scheduler and PI state when resuming
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_frameCount     = 0;
        m_piIntegral     = 0.0;
        m_piSpeedTrim    = 0.0;
        m_avgGap         = 0.0;
        requestNextFrame();
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
    // PI controller: measures DSP ring-buffer fill vs. target and returns a
    // fractional speed correction in the range [-PI_MAX_TRIM, +PI_MAX_TRIM].
    //
    // error > 0  → buffer is below target → emulator is too slow → speed up
    // error < 0  → buffer is above target → emulator is too fast → slow down
    //
    // After Phase 2 the systematic 4.3% drift is gone, so this controller only
    // needs to handle residual crystal-oscillator mismatch (~100ppm) and random
    // jitter.  The very small gains (KP=1e-5, KI=5e-7) reflect this.

    if (m_audioBackend != QtAudio || !m_audioEnabled || !m_audioOutput)
        return 0.0;

    int bufferedBytes   = (m_dspWritePos - m_dspReadPos + m_dspBufferBytes) % m_dspBufferBytes;
    int bufferedSamples = bufferedBytes / m_bytesPerSample;
    double error        = static_cast<double>(m_targetDelay - bufferedSamples);

    // On Linux/PipeWire, bytesFree() can be stale; use a longer averaging window
#ifdef __linux__
    constexpr double alpha = 0.02;  // ~50-frame window
#else
    constexpr double alpha = 0.05;  // ~20-frame window
#endif
    m_avgGap = m_avgGap * (1.0 - alpha) + error * alpha;

    // Proportional term: immediate response to current smoothed error
    double pTerm = PI_KP * m_avgGap;

    // Integral term: slow drift correction (wind-up limited by PI_MAX_TRIM)
    m_piIntegral += PI_KI * m_avgGap;
    m_piIntegral = qBound(-PI_MAX_TRIM, m_piIntegral, PI_MAX_TRIM);

    double correction = qBound(-PI_MAX_TRIM, pTerm + m_piIntegral, PI_MAX_TRIM);
    return correction;
}

void AtariEmulator::updateEmulationSpeed()
{
    // Don't touch timing when unlimited speed is requested
    if (m_userRequestedSpeedMultiplier == 0.0)
        return;

    if (!m_audioEnabled || m_audioBackend != QtAudio) {
        // No audio sync — reset PI state and use base frame time
        m_piIntegral   = 0.0;
        m_piSpeedTrim  = 0.0;
        m_currentSpeed = m_userRequestedSpeedMultiplier;
        m_targetSpeed  = m_currentSpeed;
        return;
    }

    // Get PI correction (fractional, e.g. +0.002 = 0.2% faster)
    double trim = calculateSpeedAdjustment();
    m_piSpeedTrim  = trim;
    m_currentSpeed = m_userRequestedSpeedMultiplier * (1.0 + trim);
    m_targetSpeed  = m_currentSpeed;

    // Feed correction into the absolute-time scheduler by adjusting the effective
    // frame time.  The scheduler in requestNextFrame() reads m_frameTimeMs, so
    // changing it here naturally shifts every future frame's target time.
    // Base frame time = 1000ms / targetFPS; divide by combined speed multiplier.
    double baseFps = static_cast<double>(m_targetFps);
    m_frameTimeMs  = static_cast<float>((1000.0 / baseFps) / m_currentSpeed);
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
    if (!initialized) {
        m_currentInput.trig0 = 0;
        m_currentInput.trig1 = 0;
        m_currentInput.joy0 = INPUT_STICK_CENTRE;
        m_currentInput.joy1 = INPUT_STICK_CENTRE;
        initialized = true;
    }
    
    // Effective presets per logical joystick (swap applies device+preset assignment)
    QString joy0Preset = m_swapJoysticks ? m_joystick1Preset : m_joystick0Preset;
    QString joy1Preset = m_swapJoysticks ? m_joystick0Preset : m_joystick1Preset;
    
    // Helper to calculate joystick position from directional key states
    auto calculateJoystickValue = [&](bool up, bool down, bool left, bool right) -> int {
        if (up && left) return INPUT_STICK_UL;
        if (up && right) return INPUT_STICK_UR;
        if (down && left) return INPUT_STICK_LL;
        if (down && right) return INPUT_STICK_LR;
        if (up) return INPUT_STICK_FORWARD;
        if (down) return INPUT_STICK_BACK;
        if (left) return INPUT_STICK_LEFT;
        if (right) return INPUT_STICK_RIGHT;
        return INPUT_STICK_CENTRE;
    };

    // --- Numpad preset: 8/2/4/6 or arrows + Numpad Enter ---
    bool numpadJoy0 = m_kbdJoy0Enabled && joy0Preset == "numpad";
    bool numpadJoy1 = m_kbdJoy1Enabled && joy1Preset == "numpad";
    if (numpadJoy0 || numpadJoy1) {
        bool isNumpadDir = (modifiers & Qt::KeypadModifier) || key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right;
        bool arrowKey = (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right);
        bool numpadKey = (key == Qt::Key_8 || key == Qt::Key_2 || key == Qt::Key_4 || key == Qt::Key_6) && (modifiers & Qt::KeypadModifier);

        if (isNumpadDir && (numpadKey || arrowKey)) {
            switch (key) {
                case Qt::Key_8:
                case Qt::Key_Up:
                    if (numpadJoy0) { joy0_up = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                    if (numpadJoy1) { joy1_up = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                    return true;
                case Qt::Key_2:
                case Qt::Key_Down:
                    if (numpadJoy0) { joy0_down = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                    if (numpadJoy1) { joy1_down = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                    return true;
                case Qt::Key_4:
                case Qt::Key_Left:
                    if (numpadJoy0) { joy0_left = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                    if (numpadJoy1) { joy1_left = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                    return true;
                case Qt::Key_6:
                case Qt::Key_Right:
                    if (numpadJoy0) { joy0_right = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                    if (numpadJoy1) { joy1_right = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                    return true;
            }
        }
        // Trigger: Numpad Enter only
        if (key == Qt::Key_Enter && (modifiers & Qt::KeypadModifier)) {
            if (numpadJoy0) { trig0State = isKeyPress; m_currentInput.trig0 = isKeyPress ? 1 : 0; }
            if (numpadJoy1) { trig1State = isKeyPress; m_currentInput.trig1 = isKeyPress ? 1 : 0; }
            return true;
        }
    }

    // --- Arrows preset: arrow keys only + Return ---
    bool arrowsJoy0 = m_kbdJoy0Enabled && joy0Preset == "arrows";
    bool arrowsJoy1 = m_kbdJoy1Enabled && joy1Preset == "arrows";
    if (arrowsJoy0 || arrowsJoy1) {
        switch (key) {
            case Qt::Key_Up:
                if (arrowsJoy0) { joy0_up = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (arrowsJoy1) { joy1_up = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_Down:
                if (arrowsJoy0) { joy0_down = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (arrowsJoy1) { joy1_down = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_Left:
                if (arrowsJoy0) { joy0_left = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (arrowsJoy1) { joy1_left = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_Right:
                if (arrowsJoy0) { joy0_right = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (arrowsJoy1) { joy1_right = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
        }
        // Trigger: Return (main Enter, not Numpad Enter)
        if (key == Qt::Key_Return && !(modifiers & Qt::KeypadModifier)) {
            if (arrowsJoy0) { trig0State = isKeyPress; m_currentInput.trig0 = isKeyPress ? 1 : 0; }
            if (arrowsJoy1) { trig1State = isKeyPress; m_currentInput.trig1 = isKeyPress ? 1 : 0; }
            return true;
        }
    }

    // --- WASD preset: W/A/S/D + Space ---
    bool wasdJoy0 = m_kbdJoy0Enabled && joy0Preset == "wasd";
    bool wasdJoy1 = m_kbdJoy1Enabled && joy1Preset == "wasd";
    if (wasdJoy0 || wasdJoy1) {
        switch (key) {
            case Qt::Key_W:
                if (wasdJoy0) { joy0_up = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (wasdJoy1) { joy1_up = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_S:
                if (wasdJoy0) { joy0_down = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (wasdJoy1) { joy1_down = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_A:
                if (wasdJoy0) { joy0_left = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (wasdJoy1) { joy1_left = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_D:
                if (wasdJoy0) { joy0_right = isKeyPress; m_currentInput.joy0 = calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right); }
                if (wasdJoy1) { joy1_right = isKeyPress; m_currentInput.joy1 = calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right); }
                return true;
            case Qt::Key_Space:
                if (wasdJoy0) { trig0State = isKeyPress; m_currentInput.trig0 = isKeyPress ? 1 : 0; }
                if (wasdJoy1) { trig1State = isKeyPress; m_currentInput.trig1 = isKeyPress ? 1 : 0; }
                return true;
            case Qt::Key_Q:
                if (wasdJoy0) m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UL : calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                if (wasdJoy1) m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UL : calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                return true;
            case Qt::Key_E:
                if (wasdJoy0) m_currentInput.joy0 = isKeyPress ? INPUT_STICK_UR : calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                if (wasdJoy1) m_currentInput.joy1 = isKeyPress ? INPUT_STICK_UR : calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                return true;
            case Qt::Key_Z:
                if (wasdJoy0) m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LL : calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                if (wasdJoy1) m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LL : calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                return true;
            case Qt::Key_C:
                if (wasdJoy0) m_currentInput.joy0 = isKeyPress ? INPUT_STICK_LR : calculateJoystickValue(joy0_up, joy0_down, joy0_left, joy0_right);
                if (wasdJoy1) m_currentInput.joy1 = isKeyPress ? INPUT_STICK_LR : calculateJoystickValue(joy1_up, joy1_down, joy1_left, joy1_right);
                return true;
        }
    }
    
    return false;
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
    joy1["keyboard_keys"] = m_swapJoysticks ? m_joystick1Preset : m_joystick0Preset;
    
    // Joystick 2
    QJsonObject joy2;
    joy2["direction"] = getDirectionName(m_currentInput.joy1);
    joy2["direction_value"] = m_currentInput.joy1;
    joy2["fire"] = (m_currentInput.trig1 == 1);  // 1 = pressed in our inverted logic
    joy2["keyboard_enabled"] = m_kbdJoy1Enabled;
    joy2["keyboard_keys"] = m_swapJoysticks ? m_joystick0Preset : m_joystick1Preset;
    
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

    if (m_joystickManager && !m_deferTimerStart) {
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
