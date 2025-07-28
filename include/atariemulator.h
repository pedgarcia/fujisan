/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef ATARIEMULATOR_H
#define ATARIEMULATOR_H

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

extern "C" {
    #include "libatari800.h"
    #include "akey.h"
    // Access the actual Atari color table
    extern int Colours_table[256];
    // Access cold/warm start functions
    extern void Atari800_Coldstart(void);
    extern void Atari800_Warmstart(void);
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
    
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);
    bool handleJoystickKeyboardEmulation(QKeyEvent* event);
    
    void coldBoot();
    void warmBoot();
    
    bool isBasicEnabled() const { return m_basicEnabled; }
    void setBasicEnabled(bool enabled) { m_basicEnabled = enabled; }
    
    bool isAltirraOSEnabled() const { return m_altirraOSEnabled; }
    void setAltirraOSEnabled(bool enabled) { m_altirraOSEnabled = enabled; }
    
    QString getMachineType() const { return m_machineType; }
    void setMachineType(const QString& machineType) { m_machineType = machineType; }
    
    QString getVideoSystem() const { return m_videoSystem; }
    void setVideoSystem(const QString& videoSystem) { m_videoSystem = videoSystem; }
    
    QString getOSRomPath() const { return m_osRomPath; }
    void setOSRomPath(const QString& path) { m_osRomPath = path; }
    
    QString getBasicRomPath() const { return m_basicRomPath; }
    void setBasicRomPath(const QString& path) { m_basicRomPath = path; }
    
    // Joystick keyboard emulation settings
    bool isKbdJoy0Enabled() const { return m_kbdJoy0Enabled; }
    void setKbdJoy0Enabled(bool enabled) { m_kbdJoy0Enabled = enabled; }
    
    bool isKbdJoy1Enabled() const { return m_kbdJoy1Enabled; }
    void setKbdJoy1Enabled(bool enabled) { m_kbdJoy1Enabled = enabled; }
    
    bool isJoysticksSwapped() const { return m_swapJoysticks; }
    void setJoysticksSwapped(bool swapped) { m_swapJoysticks = swapped; }
    
    float getTargetFPS() const { return m_targetFps; }
    float getFrameTimeMs() const { return m_frameTimeMs; }
    
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
    void clearInput();
    
    // Debug/execution control
    void pauseEmulation();
    void resumeEmulation();
    bool isEmulationPaused() const;
    void stepOneFrame();

public slots:
    void processFrame();

signals:
    void diskActivity(int driveNumber, bool isWriting);  // Legacy blinking
    void diskIOStart(int driveNumber, bool isWriting);   // Turn LED ON
    void diskIOEnd(int driveNumber);                     // Turn LED OFF
    void frameReady();

private:
    unsigned char convertQtKeyToAtari(int key, Qt::KeyboardModifiers modifiers);
    char getShiftedSymbol(int key, bool shiftPressed);
    void setupAudio();
    void triggerDiskActivity();
    
    bool m_basicEnabled = true;
    bool m_altirraOSEnabled = false;
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
    
    // Disk drive tracking
    QString m_diskImages[8]; // Paths for D1: through D8:
    
    // Disk I/O detection using libatari800 API
    QSet<int> m_mountedDrives;
    
    // Audio components
    QAudioOutput* m_audioOutput;
    QIODevice* m_audioDevice;
    bool m_audioEnabled;
    
    // Printer components
    bool m_printerEnabled;
    std::function<void(const QString&)> m_printerOutputCallback;
    
    // FujiNet delayed restart mechanism (matches Atari800MacX timing)
    bool m_fujinet_restart_pending;
    int m_fujinet_restart_delay;
};

#endif // ATARIEMULATOR_H