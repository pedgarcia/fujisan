/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifdef HAVE_SDL2_JOYSTICK

#include "sdl2joystickmanager.h"
#include <SDL.h>
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

// Atari INPUT_STICK constants (from atari800/src/input.h)
// libatari800 XORs these with 0xff, so we pre-calculate the inverted values
const int INPUT_STICK_CENTRE = 0x0f ^ 0xff;   // Center: 0x0f -> 0xf0
const int INPUT_STICK_LEFT = 0x0b ^ 0xff;     // Left: 0x0b -> 0xf4
const int INPUT_STICK_RIGHT = 0x07 ^ 0xff;    // Right: 0x07 -> 0xf8
const int INPUT_STICK_UP = 0x0e ^ 0xff;       // Up: 0x0e -> 0xf1
const int INPUT_STICK_DOWN = 0x0d ^ 0xff;     // Down: 0x0d -> 0xf2
const int INPUT_STICK_UL = 0x0a ^ 0xff;       // Up+Left: 0x0a -> 0xf5
const int INPUT_STICK_UR = 0x06 ^ 0xff;       // Up+Right: 0x06 -> 0xf9
const int INPUT_STICK_LL = 0x09 ^ 0xff;       // Down+Left: 0x09 -> 0xf6
const int INPUT_STICK_LR = 0x05 ^ 0xff;       // Down+Right: 0x05 -> 0xfa

SDL2JoystickManager::SDL2JoystickManager(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
    , m_deadZone(DEFAULT_DEADZONE)
    , m_enabled(false)
    , m_initialized(false)
{
    // Set up polling timer
    m_pollTimer->setInterval(POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &SDL2JoystickManager::onPollTimer);

    qDebug() << "SDL2JoystickManager created with deadzone:" << m_deadZone;
}

SDL2JoystickManager::~SDL2JoystickManager()
{
    shutdown();
}

bool SDL2JoystickManager::initialize()
{
    if (m_initialized) {
        qDebug() << "SDL2JoystickManager already initialized";
        return true;
    }

    // Check if SDL is already initialized (main system might have initialized it)
    bool sdlAlreadyInit = SDL_WasInit(SDL_INIT_JOYSTICK);

    if (!sdlAlreadyInit) {
        // Initialize SDL joystick subsystem
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
            qWarning() << "Failed to initialize SDL joystick subsystem:" << SDL_GetError();
            return false;
        }
        qDebug() << "SDL joystick subsystem initialized";
    } else {
        qDebug() << "SDL joystick subsystem already initialized";
    }

    // Verify SDL joystick subsystem is ready
    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        qWarning() << "SDL joystick subsystem verification failed";
        return false;
    }

    // Enable joystick events
    SDL_JoystickEventState(SDL_ENABLE);

    m_initialized = true;
    qDebug() << "SDL2JoystickManager initialized successfully";

    // Scan for existing joysticks
    refreshJoysticks();

    return true;
}

void SDL2JoystickManager::shutdown()
{
    if (!m_initialized) {
        return;
    }

    // Stop polling
    setEnabled(false);

    // Close all joysticks
    closeAllJoysticks();

    // Shutdown SDL joystick subsystem only if it's still initialized
    if (SDL_WasInit(SDL_INIT_JOYSTICK)) {
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }

    m_initialized = false;
    qDebug() << "SDL2JoystickManager shut down";
}

void SDL2JoystickManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (m_enabled && m_initialized) {
        refreshJoysticks();
        m_pollTimer->start();
        qDebug() << "SDL2JoystickManager polling enabled";
    } else {
        m_pollTimer->stop();
        qDebug() << "SDL2JoystickManager polling disabled";
    }
}

bool SDL2JoystickManager::isEnabled() const
{
    return m_enabled;
}

void SDL2JoystickManager::setDeadZone(int deadZone)
{
    m_deadZone = qBound(0, deadZone, 32767);
    qDebug() << "SDL2JoystickManager deadzone set to:" << m_deadZone;
}

int SDL2JoystickManager::getDeadZone() const
{
    return m_deadZone;
}

void SDL2JoystickManager::onPollTimer()
{
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Verify we're on the main thread (critical for macOS)
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        qWarning() << "SDL2JoystickManager: onPollTimer called from wrong thread!"
                   << "Current:" << QThread::currentThread()
                   << "Main:" << QCoreApplication::instance()->thread();
        return;
    }

    pollJoysticks();
}

void SDL2JoystickManager::pollJoysticks()
{
    if (!m_initialized || !SDL_WasInit(SDL_INIT_JOYSTICK)) {
        return;
    }

    // First pump events to update the SDL event queue
    SDL_PumpEvents();

    // Process only joystick device events without consuming other events
    SDL_Event events[32];  // Buffer for multiple events
    int numEvents = SDL_PeepEvents(events, 32, SDL_GETEVENT, SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED);

    for (int i = 0; i < numEvents; ++i) {
        const SDL_Event& event = events[i];
        if (event.type == SDL_JOYDEVICEADDED) {
            qDebug() << "SDL2JoystickManager: Joystick device added (hot-plug)";
            handleJoystickAdded(event.jdevice.which);
        } else if (event.type == SDL_JOYDEVICEREMOVED) {
            qDebug() << "SDL2JoystickManager: Joystick device removed (hot-unplug)";
            handleJoystickRemoved(event.jdevice.which);
        }
    }

    // Update SDL joystick state (this doesn't consume events)
    SDL_JoystickUpdate();

    // Poll each connected joystick
    for (auto it = m_joysticks.begin(); it != m_joysticks.end(); ++it) {
        int index = it.key();
        SDL_Joystick* joystick = it.value();

        if (!joystick || !SDL_JoystickGetAttached(joystick)) {
            continue;
        }

        JoystickState newState;
        newState.connected = true;
        newState.name = m_joystickStates[index].name;  // Preserve name

        // Read analog axes (assuming axes 0,1 are left stick X,Y)
        if (SDL_JoystickNumAxes(joystick) >= 2) {
            int xAxis = SDL_JoystickGetAxis(joystick, 0);
            int yAxis = SDL_JoystickGetAxis(joystick, 1);
            newState.stick = convertSDLAxisToAtariStick(xAxis, yAxis);
        } else {
            newState.stick = INPUT_STICK_CENTRE;
        }

        // Read trigger button (button 0 is typically primary button)
        if (SDL_JoystickNumButtons(joystick) > 0) {
            newState.trigger = SDL_JoystickGetButton(joystick, 0) != 0;
        } else {
            newState.trigger = false;
        }

        // Check if state changed
        if (!m_joystickStates.contains(index) ||
            m_joystickStates[index].stick != newState.stick ||
            m_joystickStates[index].trigger != newState.trigger) {

            m_joystickStates[index] = newState;
            emit joystickStateChanged(index, newState);
        }
    }
}

void SDL2JoystickManager::refreshJoysticks()
{
    if (!m_initialized) {
        qDebug() << "SDL2JoystickManager: Cannot refresh joysticks - not initialized";
        return;
    }

    // Verify SDL joystick subsystem is initialized
    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        qWarning() << "SDL2JoystickManager: SDL joystick subsystem not initialized";
        return;
    }

    // Process any pending hot-plug events before scanning
    SDL_PumpEvents();

    // Close existing joysticks
    closeAllJoysticks();

    // Scan for joysticks
    int numJoysticks = SDL_NumJoysticks();
    qDebug() << "SDL2JoystickManager: Found" << numJoysticks << "joystick(s)";

    for (int sdlIndex = 0; sdlIndex < numJoysticks && m_joysticks.size() < MAX_JOYSTICKS; ++sdlIndex) {
        SDL_Joystick* joystick = SDL_JoystickOpen(sdlIndex);
        if (joystick) {
            int ourIndex = m_joysticks.size();  // Use sequential indexing
            SDL_JoystickID instanceId = SDL_JoystickInstanceID(joystick);

            m_joysticks[ourIndex] = joystick;
            m_joystickIds[ourIndex] = instanceId;

            JoystickState state;
            state.connected = true;
            state.name = QString::fromUtf8(SDL_JoystickName(joystick));
            state.stick = INPUT_STICK_CENTRE;
            state.trigger = false;

            m_joystickStates[ourIndex] = state;

            qDebug() << "SDL2JoystickManager: Opened joystick" << ourIndex
                     << "(" << state.name << ") with"
                     << SDL_JoystickNumAxes(joystick) << "axes and"
                     << SDL_JoystickNumButtons(joystick) << "buttons";

            emit joystickConnected(ourIndex, state.name);
        } else {
            qWarning() << "SDL2JoystickManager: Failed to open joystick" << sdlIndex << ":" << SDL_GetError();
        }
    }
}

void SDL2JoystickManager::closeAllJoysticks()
{
    // Only close joysticks if SDL is still initialized
    bool sdlInitialized = SDL_WasInit(SDL_INIT_JOYSTICK);

    for (auto it = m_joysticks.begin(); it != m_joysticks.end(); ++it) {
        int index = it.key();
        SDL_Joystick* joystick = it.value();

        if (joystick && sdlInitialized) {
            SDL_JoystickClose(joystick);
            emit joystickDisconnected(index);
        }
    }

    m_joysticks.clear();
    m_joystickIds.clear();
    m_joystickStates.clear();
}

JoystickState SDL2JoystickManager::getJoystickState(int joystickIndex) const
{
    if (m_joystickStates.contains(joystickIndex)) {
        return m_joystickStates[joystickIndex];
    }

    // Return disconnected state
    return JoystickState();
}

bool SDL2JoystickManager::isJoystickConnected(int joystickIndex) const
{
    return m_joystickStates.contains(joystickIndex) &&
           m_joystickStates[joystickIndex].connected;
}

QString SDL2JoystickManager::getJoystickName(int joystickIndex) const
{
    if (m_joystickStates.contains(joystickIndex)) {
        return m_joystickStates[joystickIndex].name;
    }
    return QString();
}

QStringList SDL2JoystickManager::getConnectedJoystickNames() const
{
    QStringList names;
    for (auto it = m_joystickStates.begin(); it != m_joystickStates.end(); ++it) {
        if (it.value().connected) {
            names << it.value().name;
        }
    }
    return names;
}

int SDL2JoystickManager::getConnectedJoystickCount() const
{
    int count = 0;
    for (auto it = m_joystickStates.begin(); it != m_joystickStates.end(); ++it) {
        if (it.value().connected) {
            count++;
        }
    }
    return count;
}

int SDL2JoystickManager::convertSDLAxisToAtariStick(int xAxis, int yAxis) const
{
    // Apply deadzone
    bool leftRight = false, upDown = false;
    bool left = false, right = false, up = false, down = false;

    if (!isWithinDeadZone(xAxis)) {
        leftRight = true;
        if (xAxis < 0) {
            left = true;
        } else {
            right = true;
        }
    }

    if (!isWithinDeadZone(yAxis)) {
        upDown = true;
        if (yAxis < 0) {
            up = true;
        } else {
            down = true;
        }
    }

    // Convert to Atari joystick values (pre-inverted for libatari800)
    if (up && left) {
        return INPUT_STICK_UL;      // Up+Left diagonal
    } else if (up && right) {
        return INPUT_STICK_UR;      // Up+Right diagonal
    } else if (down && left) {
        return INPUT_STICK_LL;      // Down+Left diagonal
    } else if (down && right) {
        return INPUT_STICK_LR;      // Down+Right diagonal
    } else if (up) {
        return INPUT_STICK_UP;      // Up only
    } else if (down) {
        return INPUT_STICK_DOWN;    // Down only
    } else if (left) {
        return INPUT_STICK_LEFT;    // Left only
    } else if (right) {
        return INPUT_STICK_RIGHT;   // Right only
    } else {
        return INPUT_STICK_CENTRE;  // Centered
    }
}

bool SDL2JoystickManager::isWithinDeadZone(int axis) const
{
    return qAbs(axis) < m_deadZone;
}

void SDL2JoystickManager::handleJoystickAdded(int sdlDeviceIndex)
{
    if (!m_initialized) {
        return;
    }

    // Find the next available slot
    int ourIndex = -1;
    for (int i = 0; i < MAX_JOYSTICKS; ++i) {
        if (!m_joysticks.contains(i)) {
            ourIndex = i;
            break;
        }
    }

    if (ourIndex == -1) {
        qWarning() << "SDL2JoystickManager: Maximum joysticks reached, cannot add device" << sdlDeviceIndex;
        return;
    }

    // Open the joystick
    SDL_Joystick* joystick = SDL_JoystickOpen(sdlDeviceIndex);
    if (joystick) {
        SDL_JoystickID instanceId = SDL_JoystickInstanceID(joystick);

        m_joysticks[ourIndex] = joystick;
        m_joystickIds[ourIndex] = instanceId;

        JoystickState state;
        state.connected = true;
        state.name = QString::fromUtf8(SDL_JoystickName(joystick));
        state.stick = INPUT_STICK_CENTRE;
        state.trigger = false;

        m_joystickStates[ourIndex] = state;

        qDebug() << "SDL2JoystickManager: Hot-plugged joystick" << ourIndex
                 << "(" << state.name << ") with"
                 << SDL_JoystickNumAxes(joystick) << "axes and"
                 << SDL_JoystickNumButtons(joystick) << "buttons";

        emit joystickConnected(ourIndex, state.name);
    } else {
        qWarning() << "SDL2JoystickManager: Failed to open hot-plugged joystick" << sdlDeviceIndex << ":" << SDL_GetError();
    }
}

void SDL2JoystickManager::handleJoystickRemoved(SDL_JoystickID instanceId)
{
    if (!m_initialized) {
        return;
    }

    // Find the joystick by instance ID
    int ourIndex = -1;
    for (auto it = m_joystickIds.begin(); it != m_joystickIds.end(); ++it) {
        if (it.value() == instanceId) {
            ourIndex = it.key();
            break;
        }
    }

    if (ourIndex == -1) {
        qWarning() << "SDL2JoystickManager: Received removal event for unknown joystick instance" << instanceId;
        return;
    }

    // Close the joystick
    SDL_Joystick* joystick = m_joysticks.value(ourIndex, nullptr);
    if (joystick) {
        SDL_JoystickClose(joystick);
    }

    // Remove from our tracking
    m_joysticks.remove(ourIndex);
    m_joystickIds.remove(ourIndex);
    m_joystickStates.remove(ourIndex);

    qDebug() << "SDL2JoystickManager: Hot-unplugged joystick" << ourIndex;

    emit joystickDisconnected(ourIndex);
}

#endif // HAVE_SDL2_JOYSTICK