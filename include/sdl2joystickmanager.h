/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef SDL2JOYSTICKMANAGER_H
#define SDL2JOYSTICKMANAGER_H

#ifdef HAVE_SDL2_JOYSTICK

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QMap>

// Forward declare SDL types to avoid including SDL headers in the header
typedef struct _SDL_Joystick SDL_Joystick;
typedef int SDL_JoystickID;

// Joystick state structure for Atari emulation
struct JoystickState {
    int stick;  // Atari stick value (INPUT_STICK_* constants)
    bool trigger;  // Trigger button state
    bool connected;  // Whether joystick is connected
    QString name;  // Joystick device name

    JoystickState() : stick(0x0f), trigger(false), connected(false) {}
};

class SDL2JoystickManager : public QObject
{
    Q_OBJECT

public:
    explicit SDL2JoystickManager(QObject *parent = nullptr);
    ~SDL2JoystickManager();

    // Initialization and cleanup
    bool initialize();
    void shutdown();

    // Joystick state polling
    void pollJoysticks();

    // Get current joystick state for emulation
    JoystickState getJoystickState(int joystickIndex) const;

    // Joystick information
    bool isJoystickConnected(int joystickIndex) const;
    QString getJoystickName(int joystickIndex) const;
    QStringList getConnectedJoystickNames() const;
    int getConnectedJoystickCount() const;

    // Configuration
    void setDeadZone(int deadZone);
    int getDeadZone() const;

    // Enable/disable joystick polling
    void setEnabled(bool enabled);
    bool isEnabled() const;

signals:
    void joystickConnected(int index, const QString &name);
    void joystickDisconnected(int index);
    void joystickStateChanged(int index, const JoystickState &state);

private slots:
    void onPollTimer();

private:
    // Internal SDL management
    void refreshJoysticks();
    void closeAllJoysticks();

    // Convert SDL values to Atari format
    int convertSDLAxisToAtariStick(int xAxis, int yAxis) const;
    bool isWithinDeadZone(int axis) const;

    // Member variables
    QTimer *m_pollTimer;
    QMap<int, SDL_Joystick*> m_joysticks;  // Map joystick index to SDL_Joystick*
    QMap<int, JoystickState> m_joystickStates;  // Current state for each joystick
    QMap<int, SDL_JoystickID> m_joystickIds;  // Map our index to SDL instance ID

    int m_deadZone;  // Analog stick deadzone (0-32767)
    bool m_enabled;  // Whether joystick polling is enabled
    bool m_initialized;  // Whether SDL joystick subsystem is initialized

    static const int MAX_JOYSTICKS = 4;  // Maximum joysticks supported
    static const int DEFAULT_DEADZONE = 8000;  // Default deadzone value
    static const int POLL_INTERVAL_MS = 16;  // ~60fps polling
};

#endif // HAVE_SDL2_JOYSTICK

#endif // SDL2JOYSTICKMANAGER_H