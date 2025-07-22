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
    // AKEY constants are already included via akey.h
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
    // Note: Display parameters are saved to profiles but not applied due to libatari800 limitations
    // FUTURE: Scanlines support (commented out - not working with current atari800 build)
    // bool initializeWithConfig(bool basicEnabled, const QString& machineType, const QString& videoSystem, const QString& artifactMode, int scanlinesPercentage, bool scanlinesInterpolation);
    void shutdown();
    
    const unsigned char* getScreen();
    bool loadFile(const QString& filename);
    
    // Disk drive functions
    bool mountDiskImage(int driveNumber, const QString& filename, bool readOnly = false);
    QString getDiskImagePath(int driveNumber) const;
    
    // Audio functions
    void enableAudio(bool enabled);
    bool isAudioEnabled() const { return m_audioEnabled; }
    void setVolume(float volume);
    
    void handleKeyPress(QKeyEvent* event);
    void handleKeyRelease(QKeyEvent* event);
    
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

public slots:
    void processFrame();

signals:
    void frameReady();

private:
    unsigned char convertQtKeyToAtari(int key, Qt::KeyboardModifiers modifiers);
    char getShiftedSymbol(int key, bool shiftPressed);
    void setupAudio();
    
    bool m_basicEnabled = true;
    bool m_altirraOSEnabled = false;
    QString m_machineType = "-xl";
    QString m_videoSystem = "-pal";
    QString m_osRomPath;
    QString m_basicRomPath;
    float m_targetFps = 59.92f;
    float m_frameTimeMs = 16.67f;
    input_template_t m_currentInput;
    QTimer* m_frameTimer;
    
    // Disk drive tracking
    QString m_diskImages[8]; // Paths for D1: through D8:
    
    // Audio components
    QAudioOutput* m_audioOutput;
    QIODevice* m_audioDevice;
    bool m_audioEnabled;
};

#endif // ATARIEMULATOR_H