#include "atariemulator.h"
#include <QDebug>
#include <QApplication>

AtariEmulator::AtariEmulator(QObject *parent)
    : QObject(parent)
    , m_frameTimer(new QTimer(this))
{
    libatari800_clear_input_array(&m_currentInput);
    
    connect(m_frameTimer, &QTimer::timeout, this, &AtariEmulator::processFrame);
}

AtariEmulator::~AtariEmulator()
{
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
    m_basicEnabled = basicEnabled;
    m_machineType = machineType;
    m_videoSystem = videoSystem;
    
    // Build argument list with machine type, video system, and BASIC setting
    QStringList argList;
    argList << "atari800";
    argList << machineType;    // -xl, -xe, -atari, -5200, etc.
    argList << videoSystem;    // -ntsc or -pal
    
    if (m_altirraOSEnabled) {
        // Use built-in Altirra ROMs
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
        // Use original Atari ROMs
        if (machineType == "-5200") {
            argList << "-5200_rom";
            argList << "/Applications/Emulators/roms/atari/atari5200.rom";
        } else {
            // Use XL/XE ROM for all 8-bit machines (400/800, XL, XE)
            argList << "-xlxe_rom";
            argList << "/Applications/Emulators/roms/atari/atarixl.rom";
        }
        
        if (basicEnabled) {
            argList << "-basic_rom";
            argList << "/Applications/Emulators/roms/atari/ataribas.rom";
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
    
    if (libatari800_init(argBytes.size(), args)) {
        qDebug() << "Emulator initialized with:" << argList.join(" ");
        m_targetFps = libatari800_get_fps();
        m_frameTimeMs = 1000.0f / m_targetFps;
        qDebug() << "Target FPS:" << m_targetFps << "Frame time:" << m_frameTimeMs << "ms";
        
        // Start the frame timer
        m_frameTimer->start(static_cast<int>(m_frameTimeMs));
        return true;
    }
    
    qDebug() << "Failed to initialize emulator with:" << argList.join(" ");
    return false;
}

void AtariEmulator::shutdown()
{
    m_frameTimer->stop();
    libatari800_exit();
}

void AtariEmulator::processFrame()
{
    // Debug what we're sending to the emulator
    if (m_currentInput.keychar != 0) {
        qDebug() << "*** SENDING TO EMULATOR: keychar=" << (int)m_currentInput.keychar << "'" << QChar(m_currentInput.keychar) << "' ***";
    }
    // Always debug L key specifically since AKEY_l = 0
    bool hasInput = m_currentInput.keychar != 0 || m_currentInput.keycode != 0 || m_currentInput.special != 0 ||
                   m_currentInput.start != 0 || m_currentInput.select != 0 || m_currentInput.option != 0;
    
    if (hasInput) {
        qDebug() << "*** SENDING TO EMULATOR: keychar=" << (int)m_currentInput.keychar 
                 << " keycode=" << (int)m_currentInput.keycode 
                 << " special=" << (int)m_currentInput.special 
                 << " start=" << (int)m_currentInput.start
                 << " select=" << (int)m_currentInput.select
                 << " option=" << (int)m_currentInput.option << " ***";
    }
    
    libatari800_next_frame(&m_currentInput);
    // Clear input after processing so it doesn't repeat
    libatari800_clear_input_array(&m_currentInput);
    
    emit frameReady();
}

const unsigned char* AtariEmulator::getScreen()
{
    return libatari800_get_screen_ptr();
}

bool AtariEmulator::loadFile(const QString& filename)
{
    bool result = libatari800_reboot_with_file(filename.toUtf8().constData());
    if (result) {
        qDebug() << "Loaded:" << filename;
    }
    return result;
}

void AtariEmulator::handleKeyPress(QKeyEvent* event)
{
    // Clear previous input
    libatari800_clear_input_array(&m_currentInput);
    
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    bool shiftPressed = modifiers & Qt::ShiftModifier;
    bool ctrlPressed = modifiers & Qt::ControlModifier;
    
    qDebug() << "Key pressed:" << key << "modifiers:" << modifiers << "Qt::Key_1:" << Qt::Key_1 << "Qt::Key_Exclam:" << Qt::Key_Exclam;
    
    // Handle Ctrl key combinations first
    if (ctrlPressed && key >= Qt::Key_A && key <= Qt::Key_Z) {
        // Ctrl+letter generates control codes (1-26)
        m_currentInput.keychar = key - Qt::Key_A + 1;
        qDebug() << "Setting keychar to: Ctrl+" << QChar(key) << "(code" << (int)m_currentInput.keychar << ")";
    } else if (ctrlPressed && key == Qt::Key_C) {
        // Ctrl+C - interrupt/break
        m_currentInput.keychar = 3; // ETX character
        qDebug() << "Setting keychar to: Ctrl+C (break)";
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
    Q_UNUSED(event)
    // Clear input when key is released
    libatari800_clear_input_array(&m_currentInput);
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