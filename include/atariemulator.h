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

extern "C" {
    #include "libatari800.h"
    #include "akey.h"
    // Access the actual Atari color table
    extern int Colours_table[256];
    // Access cold/warm start functions
    extern void Atari800_Coldstart(void);
    extern void Atari800_Warmstart(void);
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
    void shutdown();
    
    const unsigned char* getScreen();
    bool loadFile(const QString& filename);
    
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
    
    float getTargetFPS() const { return m_targetFps; }
    float getFrameTimeMs() const { return m_frameTimeMs; }

public slots:
    void processFrame();

signals:
    void frameReady();

private:
    unsigned char convertQtKeyToAtari(int key, Qt::KeyboardModifiers modifiers);
    char getShiftedSymbol(int key, bool shiftPressed);
    
    bool m_basicEnabled = true;
    bool m_altirraOSEnabled = false;
    QString m_machineType = "-xl";
    QString m_videoSystem = "-pal";
    float m_targetFps = 59.92f;
    float m_frameTimeMs = 16.67f;
    input_template_t m_currentInput;
    QTimer* m_frameTimer;
};

#endif // ATARIEMULATOR_H