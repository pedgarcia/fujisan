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
#include <QImage>
#include <atomic>

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
    Q_INVOKABLE void shutdown();
    
    const unsigned char* getScreen();
    bool loadFile(const QString& filename);
    void ejectCartridge();

    // Disk drive functions
    bool mountDiskImage(int driveNumber, const QString& filename, bool readOnly = false);
    void dismountDiskImage(int driveNumber);
    void disableDrive(int driveNumber);
    Q_INVOKABLE void coldRestart();
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

    /// Master "enable joystick" (toolbar / settings). When false, SDL sticks and kbd-joy maps are inactive.
    bool isJoystickInputEnabled() const { return m_joystickInputEnabled.load(); }
    
    Q_INVOKABLE void coldBoot();
    Q_INVOKABLE void warmBoot();
    Q_INVOKABLE void resetNetSIOClientState();
    Q_INVOKABLE void ensureNetsioEnabled();

    /// Tear down audio output objects (Qt audio, SDL backends). Must be called on the
    /// thread that owns the emulator (the worker thread during normal operation).
    void teardownAudio();
    /// Runs shutdown(), teardownAudio(), clears deferred QMetaCallEvents for this object,
    /// moveToThread(GUI), then quits the worker thread. Must be invoked with
    /// Qt::QueuedConnection from the GUI thread after requestShutdown() so timers are
    /// stopped on the worker and the object tree is re-homed before delete on the GUI.
    Q_INVOKABLE void finalizeShutdownOnWorkerAndRehomeToGui();
    /// Signal the emulator to stop processing frames. Safe to call from any thread.
    /// Then queue finalizeShutdownOnWorkerAndRehomeToGui() on the worker.
    void requestShutdown() { m_shuttingDown.store(true); }
    /// Call from the GUI thread during quit before waiting on the worker. Unblocks
    /// netsio_recv_byte() / select() so finalizeShutdown can run (requires atari800 0018 patch).
    /// Safe to call even when NetSIO is toggled off: netsio_shutdown() is a no-op if uninitialized.
    void netsioShutdownFromOtherThreadForQuit();
    void setDeferTimerStart(bool defer) { m_deferTimerStart = defer; }
    void startDeferredTimers();
    Q_INVOKABLE bool shouldAutoColdBootForFujiNet();
    bool isPendingFujiNetBoot() const { return m_pendingFujiNetBoot; }
    bool updateHardDrivePath(int driveNumber, const QString& path);
    
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

    /// True after a successful libatari800_init() until shutdown() clears the core.
    /// Used to avoid deferred Qt callbacks touching lib globals after libatari800_exit().
    bool isLibatari800Initialized() const { return m_libatari800Initialized; }
    
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

    void setJoystick0Preset(const QString& preset);
    void setJoystick1Preset(const QString& preset);
    QString getJoystick0Preset() const { return m_joystick0Preset; }
    QString getJoystick1Preset() const { return m_joystick1Preset; }

public slots:
    /// Apply joystick master flag, per-port devices, kbd-joy flags, swap, and presets (must run on emulator thread).
    void applyJoystickInputBundle(bool master, const QString& device1, const QString& device2,
                                  bool kbd0, bool kbd1, bool swap,
                                  const QString& preset0, const QString& preset1);

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
    Q_INVOKABLE bool saveState(const QString& filename);
    Q_INVOKABLE bool loadState(const QString& filename);
    Q_INVOKABLE bool quickSaveState();
    Q_INVOKABLE bool quickLoadState();
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
    void setAudioDiagnosticsEnabled(bool enabled) { m_enableAudioDiagnostics = enabled; }
    void requestNextFrame();  // Public so timer callback can schedule the next frame
    
    // Direct input injection for paste functionality
    void injectCharacter(char ch);
    
    // XEX loading for debugging - loads and sets entry point breakpoint
    bool loadXexForDebug(const QString& filename);
    
    void injectAKey(int akeyCode);  // For raw AKEY code injection
    void clearInput();

    /// Thread-safe; for unit tests verifying paste/TCP injection (see test_character_injection).
    int injectedKeyFramesRemainingForTest() const;
    /// Thread-safe. True when inject hold/post-release counters are zero (ignores OS CH).
    /// Use in unit tests; production paste uses isCharacterInjectionIdle() which also polls CH.
    bool injectionTimersIdleForTest() const;
    /// Thread-safe. False while hold/post-release frames are pending, or until CH ($02FC) is clear
    /// (OS keyboard buffer) when libatari800 is initialized — see kInjectPostReleaseFrameCount.
    bool isCharacterInjectionIdle() const;

    // Caps lock control
    void setCapsLock(bool enabled);  // Set caps lock to specific state
    bool getCapsLockState() const;   // Get current caps lock state

    // Screen memory inspection
    bool isConfigDriveSlotsScreen() const;  // Check if FujiNet CONFIG drive slots screen is displayed

    // Debug/execution control
    Q_INVOKABLE void pauseEmulation();
    Q_INVOKABLE void resumeEmulation();
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
    void frameReady(const QImage& image);
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
    /// Caller must hold m_inputMutex. Uses lib clear only while the core is initialized.
    void clearCurrentInputLocked();
    void triggerDiskActivity();
    QString quotePath(const QString& path);  // Helper to quote paths with spaces
    bool m_enableAudioDiagnostics = false;   // Enable CSV logging for audio diagnostics

    // Frame rendering: converts libatari800 screen buffer to a QImage on the emulator thread.
    // Called at the end of processFrame() before emitting frameReady().
    QImage renderFrameImage();

    // Protects m_currentInput against concurrent access between the main thread
    // (keyboard/joystick events) and the emulator thread (processFrame snapshot).
    mutable QMutex m_inputMutex;
    // Injected keys (paste / TCP) must only advance libatari800 on the emulator thread.
    static constexpr int kInjectKeyHoldFrameCount = 3;
    // After the hold, require this many frames at AKEY_NONE (same order as Atari800MacX paste:
    // POKEY XOR debounce needs multiple idle frames before the same scancode re-latches).
    static constexpr int kInjectPostReleaseFrameCount = 3;
    int m_injectKeyFramesRemaining = 0;
    int m_injectPostReleaseFrames = 0;

    // Reusable frame image buffer; its shared data is detached (copy-on-write) each
    // time the emulator thread writes a new frame while the previous one is still
    // referenced by the main-thread signal queue.
    QImage m_frameImage;

    // High-resolution frame timing using absolute time scheduling.
    // Each frame is scheduled at firstFrameTime + frameCount * frameTimeMs,
    // so Qt timer overshoot/undershoot is automatically compensated next frame.
    std::chrono::steady_clock::time_point m_firstFrameTime;
    int64_t m_frameCount = 0;
    
    bool m_basicEnabled = true;
    bool m_altirraOSEnabled = false;
    bool m_altirraBASICEnabled = false;
    QString m_machineType = "-xl";
    QString m_videoSystem = "-pal";
    QString m_osRomPath;
    QString m_basicRomPath;
    
    // Joystick keyboard emulation settings
    std::atomic<bool> m_joystickInputEnabled{true};
    bool m_kbdJoy0Enabled = false;  // Default false - keyboard joysticks disabled by default
    bool m_kbdJoy1Enabled = false;  // Default false - keyboard joysticks disabled by default
    bool m_swapJoysticks = false;   // Default false: Joy0=Numpad, Joy1=WASD
    QString m_joystick0Preset = "numpad";  // "numpad" | "arrows" | "wasd"
    QString m_joystick1Preset = "wasd";
    float m_targetFps = 59.92f;
    float m_frameTimeMs = 16.67f;
    input_template_t m_currentInput;
    bool m_capsLockEnabled = false;  // Track caps lock state
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
    // Staging buffer: assembles a contiguous chunk for one write() to QIODevice.
    // Must be able to hold the largest single write: min(DSP ring span, Qt bytesFree()),
    // which on Linux is often up to the QAudioOutput buffer size (8192), not merely
    // one emulator frame (~1472 bytes). Code resizes at runtime if needed.
    static constexpr int DSP_STAGING_BYTES = 8192;
    QByteArray m_dspStagingBuffer;
    qint64 m_callbackTick;  // Time when callback occurred
    double m_avgGap;     // Average gap for speed adjustment
    int m_targetDelay;   // Target delay in samples
    int m_sampleRate;
    int m_bytesPerSample;
    int m_fragmentSize;  // Size of each audio fragment

    // PI controller for audio clock feedback (Phase 3)
    // Corrections are tiny (±0.5%) so there are no audible pitch changes.
    double m_piIntegral = 0.0;      // Integral term accumulator
    double m_piSpeedTrim = 0.0;     // Current speed trim applied to m_frameTimeMs

    static constexpr double PI_KP      = 0.000010;  // Proportional gain
    static constexpr double PI_KI      = 0.0000005; // Integral gain
    static constexpr double PI_MAX_TRIM = 0.005;    // ±0.5% max correction

    // User-requested speed multiplier (separate from audio sync adjustment)
    double m_userRequestedSpeedMultiplier;  // 1.0 = normal, 2.0 = 2x, 0.5 = 0.5x, 0.0 = unlimited
    double m_currentSpeed;  // Effective combined speed (user × PI trim) — kept for legacy API
    double m_targetSpeed;   // Target speed (kept for legacy API)
    
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

    // NetSIO/FujiNet state tracking
    bool m_netSIOEnabled;
    // Set when the emulator booted with netSIOEnabled=true but FujiNet-PC was not
    // yet connected (netsio_enabled=0).  Cleared by coldBoot().  Checked by
    // MainWindow::onFujiNetConnected() to trigger an automatic cold boot once
    // FujiNet-PC is confirmed ready, so the Atari can actually load from it.
    bool m_pendingFujiNetBoot = false;
    bool m_libatari800Initialized = false;  // Tracks whether libatari800_init() succeeded; reset by shutdown() to prevent double libatari800_exit()
    std::atomic<bool> m_shuttingDown{false}; // Set from GUI thread to interrupt processFrame before cleanup lambda runs
    bool m_deferTimerStart = false;          // When true, requestNextFrame() records intent but doesn't start the timer (call startDeferredTimers() on the worker later)

    // Current profile name for state saves
    QString m_currentProfileName;
    
    // Partial frame execution for precise breakpoints
    bool m_usePartialFrameExecution;
    int m_cyclesThisFrame;
    static constexpr int CYCLES_PER_FRAME = 29833;  // NTSC: 29833, PAL: 35568
    static constexpr int BREAKPOINT_CHECK_CHUNK = 500;  // Check every 500 cycles
    
};

#endif // ATARIEMULATOR_H