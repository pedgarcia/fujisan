/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef ATARIEMULATOR_H
#define ATARIEMULATOR_H

#ifdef _WIN32
#include "../src/windows_compat.h"
#endif

#include <QObject>
#include <QTimer>
#include <QKeyEvent>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QIODevice>
#include <QBuffer>
#include <functional>
#include <QSet>
#include <QJsonObject>
#include <QMutex>

#ifdef HAVE_SDL2_AUDIO
// Forward declaration to avoid including SDL headers here
class SDL2AudioBackend;
#endif

// Forward declaration for unified audio backend
class UnifiedAudioBackend;

#ifdef HAVE_SDL2_JOYSTICK
// Forward declaration for joystick manager
class SDL2JoystickManager;
struct JoystickState;
#endif

extern "C" {
    // Temporarily undefine potentially conflicting macros
    #ifdef string
    #define TEMP_STRING_BACKUP string
    #undef string
    #endif
    
    #include "libatari800.h"
    #include "akey.h"
    // Access the actual Atari color table
    extern int Colours_table[256];
    // Access cold/warm start functions
    extern void Atari800_Coldstart(void);
    extern void Atari800_Warmstart(void);
    // Binary loading status
    extern int BINLOAD_start_binloading;
    // Disk mounting functions
    extern int libatari800_mount_disk_image(int diskno, const char *filename, int readonly);
    // Memory access
    extern unsigned char* libatari800_get_main_memory_ptr();
    // SIO monitoring variables
    extern int SIO_last_op;
    extern int SIO_last_op_time;
    extern int SIO_last_drive;
    #define SIO_LAST_READ 0
    #define SIO_LAST_WRITE 1
    // Cartridge functions
    extern void CARTRIDGE_RemoveAutoReboot(void);
    extern int CARTRIDGE_InsertAutoReboot(const char *filename);
    extern int CARTRIDGE_autoreboot;
    // Cartridge type and configuration
    typedef struct {
        int type;
        int state;
        int size;
        unsigned char *image;
        char filename[260];  // FILENAME_MAX
        int raw;
    } CARTRIDGE_image_t;
    extern CARTRIDGE_image_t CARTRIDGE_main;
    extern void CARTRIDGE_SetTypeAutoReboot(CARTRIDGE_image_t *cart, int type);
    // Cartridge type constants
    #define CARTRIDGE_STD_8 1
    // BASIC control (needed for cartridge loading)
    extern int Atari800_disable_basic;
    // Audio functions
    extern unsigned char* libatari800_get_sound_buffer(void);
    extern int libatari800_get_sound_buffer_len(void);
    extern int libatari800_get_sound_frequency(void);
    extern int libatari800_get_num_sound_channels(void);
    extern int libatari800_get_sound_sample_size(void);
    // Color adjustment functions and structures
    #include "colours.h"
    extern Colours_setup_t COLOURS_NTSC_setup;
    extern Colours_setup_t COLOURS_PAL_setup;
    extern void Colours_Update(void);
    // Artifact functions
    #include "artifact.h"
    extern void ARTIFACT_Set(ARTIFACT_t mode);
    // FUTURE: TV Scanlines Implementation Notes
    // 
    // Scanlines are currently not working. Investigation needed:
    // 1. Command line parameters (-scanlines, -scanlinesint) don't work with current atari800 build
    // 2. May require full SDL2 build instead of libatari800 (minimal library)
    // 3. Possible alternatives:
    //    - Build atari800 with --with-sdl2 --with-opengl flags
    //    - Implement client-side Qt overlay using QPainter with scanline effect
    //    - Use different atari800 parameters or build configuration
    //    - Investigate if specific SDL video features need to be enabled
    //
    // Current status: All scanlines code commented out until proper implementation found
    // Speed control variables
    extern int Atari800_turbo;
    extern int Atari800_turbo_speed;
    // CPU registers for debugging
    extern unsigned short CPU_regPC;
    extern unsigned char CPU_regA;
    extern unsigned char CPU_regX;
    extern unsigned char CPU_regY;
    extern unsigned char CPU_regS;
    extern unsigned char CPU_regP;
    // AKEY constants are already included via akey.h
    // Printer/Device functions
    #include "devices.h"
    extern char Devices_print_command[256];
    extern int Devices_SetPrintCommand(const char *command);
    // State save functions
    #include "statesav.h"
    
    // Disk activity callback function
    extern void libatari800_set_disk_activity_callback(void (*callback)(int drive, int operation));
    
    // Single-instruction stepping for debugger
    extern void libatari800_step_instruction(void);
    
    // NOTE: libatari800_exit and Atari800_InitialiseMachine are already declared
    // in libatari800.h and atari.h respectively, so we don't redeclare them here
    
    // Restore string macro if it was defined
    #ifdef TEMP_STRING_BACKUP
    #define string TEMP_STRING_BACKUP
    #undef TEMP_STRING_BACKUP
    #endif
}

class AtariEmulator : public QObject
{
    Q_OBJECT

public:
    explicit AtariEmulator(QObject *parent = nullptr);
    ~AtariEmulator();

    bool initialize();
    bool initializeWithBasic(bool basicEnabled);
    bool initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem);
    bool initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode);
    bool initializeWithDisplayConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                   const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                   const QString& fitScreen, bool show80Column, bool vSyncEnabled);
    bool initializeWithInputConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                 const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                 const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                 bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks, bool netSIOEnabled = false);
    bool initializeWithNetSIOConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode,
                                  const QString& horizontalArea, const QString& verticalArea, int horizontalShift, int verticalShift,
                                  const QString& fitScreen, bool show80Column, bool vSyncEnabled,
                                  bool kbdJoy0Enabled, bool kbdJoy1Enabled, bool swapJoysticks,
                                  bool netSIOEnabled, bool rtimeEnabled);
    // Note: Display parameters are saved to profiles but not applied due to libatari800 limitations
    // FUTURE: Scanlines support (commented out - not working with current atari800 build)
    // bool initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode, int scanlinesPercentage, bool scanlinesInterpolation);
    void shutdown();
    
    const unsigned char* getScreen();
    bool loadFile(const QString& filename);
    void ejectCartridge();

    // Disk drive functions
    bool mountDiskImage(int driveNumber, const QString& filename, bool readOnly = false);
    void dismountDiskImage(int driveNumber);
    void disableDrive(int driveNumber);
    void coldRestart();
    QString getDiskImagePath(int driveNumber) const;
    
    // Printer functions
    void setPrinterEnabled(bool enabled);
    bool isPrinterEnabled() const;
    void setPrinterOutputCallback(std::function<void(const QString&)> callback);
    void setPrintCommand(const QString& command);
    
    // Disk I/O monitoring
    void setDiskActivityCallback(std::function<void(int, bool)> callback); // drive, isWriting
    
    // SIO patch control (disk speed)
    bool getSIOPatchEnabled() const;
    bool setSIOPatchEnabled(bool enabled);
    void debugSIOPatchStatus() const;
    
    // Audio functions
    void enableAudio(bool enabled);
    bool isAudioEnabled() const { return m_audioEnabled; }
    void setVolume(float volume);
    
    // Audio backend selection
    enum AudioBackend {
        QtAudio,          // Legacy Qt audio (deprecated)
        SDL2Audio,        // Legacy SDL2 audio (deprecated)
        UnifiedAudio      // New unified audio backend
    };
    void setAudioBackend(AudioBackend backend);
    AudioBackend getAudioBackend() const { return m_audioBackend; }
    
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);
    bool handleJoystickKeyboardEmulation(QKeyEvent* event);
    
    void coldBoot();
    void warmBoot();
    
    bool isBasicEnabled() const { return m_basicEnabled; }
    void setBasicEnabled(bool enabled) { m_basicEnabled = enabled; }
    
    bool isAltirraOSEnabled() const { return m_altirraOSEnabled; }
    void setAltirraOSEnabled(bool enabled) { m_altirraOSEnabled = enabled; }
    
    bool isAltirraBASICEnabled() const { return m_altirraBASICEnabled; }
    void setAltirraBASICEnabled(bool enabled) { m_altirraBASICEnabled = enabled; }
    
    QString getMachineType() const { return m_machineType; }
    void setMachineType(const QString& machineType) { m_machineType = machineType; }
    
    QString getVideoSystem() const { return m_videoSystem; }
    void setVideoSystem(const QString& videoSystem) { m_videoSystem = videoSystem; }
    int getCurrentEmulationSpeed() const;
    
    QString getOSRomPath() const { return m_osRomPath; }
    void setOSRomPath(const QString& path) { m_osRomPath = path; }
    
    QString getBasicRomPath() const { return m_basicRomPath; }
    void setBasicRomPath(const QString& path) { m_basicRomPath = path; }
    
    // Joystick keyboard emulation settings
    bool isKbdJoy0Enabled() const { return m_kbdJoy0Enabled; }
    void setKbdJoy0Enabled(bool enabled);
    
    bool isKbdJoy1Enabled() const { return m_kbdJoy1Enabled; }
    void setKbdJoy1Enabled(bool enabled);
    
    bool isJoysticksSwapped() const { return m_swapJoysticks; }
    void setJoysticksSwapped(bool swapped) { m_swapJoysticks = swapped; }

#ifdef HAVE_SDL2_JOYSTICK
    // Real joystick (SDL2) support
    bool isRealJoysticksEnabled() const { return m_realJoysticksEnabled; }
    void setRealJoysticksEnabled(bool enabled);
    SDL2JoystickManager* getJoystickManager() const { return m_joystickManager; }

    // Device assignment methods
    void setJoystick1Device(const QString& device);
    void setJoystick2Device(const QString& device);
    QString getJoystick1Device() const { return m_joystick1AssignedDevice; }
    QString getJoystick2Device() const { return m_joystick2AssignedDevice; }
#endif
    
    float getTargetFPS() const { return m_targetFps; }
    float getFrameTimeMs() const { return m_frameTimeMs; }
    
    // Joystick control methods for TCP server
    void setJoystickState(int player, int direction, bool fire);
    void releaseJoystick(int player);
    
    // Joystick monitoring methods for TCP server
    int getJoystickState(int player) const;
    bool getJoystickFire(int player) const;
    QJsonObject getAllJoystickStates() const;
    
    // State save/load methods
    bool saveState(const QString& filename);
    bool loadState(const QString& filename);
    bool quickSaveState();
    bool quickLoadState();
    QString getQuickSaveStatePath() const;
    void setCurrentProfileName(const QString& profileName) { m_currentProfileName = profileName; }
    QString getCurrentProfileName() const { return m_currentProfileName; }
    
    // Color adjustment methods
    void updateColorSettings(bool isPal, double saturation, double contrast, double brightness, double gamma, double hue);
    void updatePalColorSettings(double saturation, double contrast, double brightness, double gamma, double hue);
    void updateNtscColorSettings(double saturation, double contrast, double brightness, double gamma, double hue);
    void updateArtifactSettings(const QString& artifactMode);
    // FUTURE: Scanlines methods (commented out - not working)
    // bool needsScanlineRestart() const;
    
    // Speed control
    void setEmulationSpeed(int percentage);
    
    // Direct input injection for paste functionality
    void injectCharacter(char ch);
    
    // XEX loading for debugging - loads and sets entry point breakpoint
    bool loadXexForDebug(const QString& filename);
    
    void injectAKey(int akeyCode);  // For raw AKEY code injection
    void clearInput();
    
    // Debug/execution control
    void pauseEmulation();
    void resumeEmulation();
    bool isEmulationPaused() const;
    void stepOneFrame();
    void stepOneInstruction();
    
    // Breakpoint management - core debugging support
    void addBreakpoint(unsigned short address);
    void removeBreakpoint(unsigned short address);
    void clearAllBreakpoints();
    bool hasBreakpoint(unsigned short address) const;
    QSet<unsigned short> getBreakpoints() const;
    void setBreakpointsEnabled(bool enabled);
    bool areBreakpointsEnabled() const;
    
    // Dynamic speed adjustment for audio sync
    double calculateSpeedAdjustment();
    void updateEmulationSpeed();

public slots:
    void processFrame();

signals:
    void diskActivity(int driveNumber, bool isWriting);  // Legacy blinking
    void diskIOStart(int driveNumber, bool isWriting);   // Turn LED ON
    void diskIOEnd(int driveNumber);                     // Turn LED OFF
    void frameReady();
    void xexLoadedForDebug(unsigned short entryPoint);
    
    // Core debugging signals
    void breakpointHit(unsigned short address);
    void breakpointAdded(unsigned short address);
    void breakpointRemoved(unsigned short address);
    void breakpointsCleared();
    void executionPaused();
    void executionResumed();
    void debugStepped();

private:
    unsigned char convertQtKeyToAtari(int key, Qt::KeyboardModifiers modifiers);
    char getShiftedSymbol(int key, bool shiftPressed);
    void setupAudio();
    void triggerDiskActivity();
    QString quotePath(const QString& path);  // Helper to quote paths with spaces
    
    bool m_basicEnabled = true;
    bool m_altirraOSEnabled = false;
    bool m_altirraBASICEnabled = false;
    QString m_machineType = "-xl";
    QString m_videoSystem = "-pal";
    QString m_osRomPath;
    QString m_basicRomPath;
    
    // Joystick keyboard emulation settings
    bool m_kbdJoy0Enabled = true;   // Default true to match SDL
    bool m_kbdJoy1Enabled = false;  // Default false to match SDL
    bool m_swapJoysticks = false;   // Default false: Joy0=Numpad, Joy1=WASD
    float m_targetFps = 59.92f;
    float m_frameTimeMs = 16.67f;
    input_template_t m_currentInput;
    QTimer* m_frameTimer;

    // Debug/execution state
    bool m_emulationPaused = false;
    
    // Core breakpoint management
    QSet<unsigned short> m_breakpoints;
    bool m_breakpointsEnabled = true;
    unsigned short m_lastPC = 0xFFFF;  // Track last PC for breakpoint detection
    void checkBreakpoints();  // Internal breakpoint checking
    
    // Disk drive tracking
    QString m_diskImages[8]; // Paths for D1: through D8:
    
    // Disk I/O detection using libatari800 API
    QSet<int> m_mountedDrives;
    
    // Audio components
    AudioBackend m_audioBackend;
    
    // Qt Audio backend
    QAudioOutput* m_audioOutput;
    QIODevice* m_audioDevice;
    bool m_audioEnabled;
    
    // Double buffering for audio (inspired by Atari800MacX)
    static const int DSP_BUFFER_FRAGS = 5;  // Number of fragments in DSP buffer
    QByteArray m_dspBuffer;
    int m_dspBufferBytes;
    int m_dspWritePos;
    int m_dspReadPos;
    qint64 m_callbackTick;  // Time when callback occurred
    double m_avgGap;     // Average gap for speed adjustment
    int m_targetDelay;   // Target delay in samples
    int m_sampleRate;
    int m_bytesPerSample;
    int m_fragmentSize;  // Size of each audio fragment
    
    // Dynamic speed adjustment (like Atari800MacX)
    double m_currentSpeed;  // Current emulation speed (0.95 to 1.05)
    double m_targetSpeed;   // Target speed based on buffer level
    static constexpr double SPEED_ADJUSTMENT_ALPHA = 0.1;  // Smoothing factor

    // User-requested speed multiplier (separate from audio sync adjustment)
    double m_userRequestedSpeedMultiplier;  // 1.0 = normal, 2.0 = 2x, 0.5 = 0.5x, 0.0 = unlimited
    
    // Unified Audio Backend (new implementation)
    UnifiedAudioBackend* m_unifiedAudio;

#ifdef HAVE_SDL2_AUDIO
    // SDL2 Audio backend (legacy)
    SDL2AudioBackend* m_sdl2Audio;

    // SDL2 audio ring buffer (legacy)
    QByteArray m_sdl2AudioBuffer;
    int m_sdl2WritePos;
    int m_sdl2ReadPos;
    QMutex m_sdl2AudioMutex;
    static const int SDL2_BUFFER_SIZE = 32768;  // 32KB ring buffer for better stability

    // Dynamic buffer management (legacy)
    int m_sdl2TargetBufferLevel;  // Target amount of data to maintain in buffer
    int m_sdl2BufferLevelAccum;   // Accumulator for averaging buffer level
    int m_sdl2BufferLevelCount;   // Count for averaging
    int m_sdl2FrameSkipCounter;   // Counter for periodic frame skipping
    bool m_sdl2AdaptiveMode;      // Whether to use adaptive timing
#endif

#ifdef HAVE_SDL2_JOYSTICK
    // SDL2 Joystick support
    SDL2JoystickManager* m_joystickManager;
    bool m_realJoysticksEnabled;

    // Device assignment tracking
    QString m_joystick1AssignedDevice;  // "keyboard", "none", or "sdl_X"
    QString m_joystick2AssignedDevice;  // "keyboard", "none", or "sdl_X"
#endif
    
    // Printer components
    bool m_printerEnabled;
    std::function<void(const QString&)> m_printerOutputCallback;
    
    // FujiNet delayed restart mechanism (matches Atari800MacX timing)
    bool m_fujinet_restart_pending;
    int m_fujinet_restart_delay;
    
    // Current profile name for state saves
    QString m_currentProfileName;
    
    // Partial frame execution for precise breakpoints
    bool m_usePartialFrameExecution;
    int m_cyclesThisFrame;
    static constexpr int CYCLES_PER_FRAME = 29833;  // NTSC: 29833, PAL: 35568
    static constexpr int BREAKPOINT_CHECK_CHUNK = 500;  // Check every 500 cycles
    
};

#endif // ATARIEMULATOR_H