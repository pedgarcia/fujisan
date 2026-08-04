#ifndef KEYBOARDJOYSTICKMAP_H
#define KEYBOARDJOYSTICKMAP_H

#include <QString>
#include <QStringList>
#include <QKeySequence>
#include <Qt>

/// Shared definitions for customizable keyboard-joystick bindings.
///
/// A single binding is encoded as an int: the Qt::Key value with bit 30 set when the
/// key is distinguished by Qt::KeypadModifier (numpad Enter/8 vs main Enter/8).
/// 0 means "unbound" (only valid for the optional diagonal bindings).
namespace KbdJoy {

static const int KeypadFlag = 0x40000000;

inline int encodeKey(int qtKey, bool keypad)
{
    if (qtKey == 0 || qtKey == Qt::Key_unknown) {
        return 0;
    }
    return keypad ? (qtKey | KeypadFlag) : qtKey;
}

inline int decodeQtKey(int encoded)
{
    return encoded & ~KeypadFlag;
}

inline bool decodeKeypad(int encoded)
{
    return (encoded & KeypadFlag) != 0;
}

/// Per-joystick keyboard map: 4 cardinals + fire (required) and 4 diagonals
/// (0 = unbound; diagonals then come from holding two cardinal keys).
struct KeyboardJoystickMap {
    int up = 0;
    int down = 0;
    int left = 0;
    int right = 0;
    int ul = 0;
    int ur = 0;
    int ll = 0;
    int lr = 0;
    int fire = 0;

    bool operator==(const KeyboardJoystickMap& o) const
    {
        return up == o.up && down == o.down && left == o.left && right == o.right
            && ul == o.ul && ur == o.ur && ll == o.ll && lr == o.lr && fire == o.fire;
    }
    bool operator!=(const KeyboardJoystickMap& o) const { return !(*this == o); }

    static KeyboardJoystickMap numpadPreset()
    {
        KeyboardJoystickMap m;
        m.up = encodeKey(Qt::Key_8, true);
        m.down = encodeKey(Qt::Key_2, true);
        m.left = encodeKey(Qt::Key_4, true);
        m.right = encodeKey(Qt::Key_6, true);
        m.fire = encodeKey(Qt::Key_Enter, true);
        return m;
    }

    static KeyboardJoystickMap arrowsPreset()
    {
        KeyboardJoystickMap m;
        m.up = encodeKey(Qt::Key_Up, false);
        m.down = encodeKey(Qt::Key_Down, false);
        m.left = encodeKey(Qt::Key_Left, false);
        m.right = encodeKey(Qt::Key_Right, false);
        m.fire = encodeKey(Qt::Key_Return, false);
        return m;
    }

    static KeyboardJoystickMap wasdPreset()
    {
        KeyboardJoystickMap m;
        m.up = encodeKey(Qt::Key_W, false);
        m.down = encodeKey(Qt::Key_S, false);
        m.left = encodeKey(Qt::Key_A, false);
        m.right = encodeKey(Qt::Key_D, false);
        m.ul = encodeKey(Qt::Key_Q, false);
        m.ur = encodeKey(Qt::Key_E, false);
        m.ll = encodeKey(Qt::Key_Z, false);
        m.lr = encodeKey(Qt::Key_C, false);
        m.fire = encodeKey(Qt::Key_Space, false);
        return m;
    }
};

inline bool isValidPresetName(const QString& preset)
{
    return preset == QStringLiteral("numpad")
        || preset == QStringLiteral("arrows")
        || preset == QStringLiteral("wasd");
}

/// Expand a preset name into its 9 bindings. Unknown names (including "custom")
/// yield the numpad layout, matching the historical default for joystick 1.
inline KeyboardJoystickMap mapForPreset(const QString& preset)
{
    if (preset == QStringLiteral("arrows")) {
        return KeyboardJoystickMap::arrowsPreset();
    }
    if (preset == QStringLiteral("wasd")) {
        return KeyboardJoystickMap::wasdPreset();
    }
    return KeyboardJoystickMap::numpadPreset();
}

/// Human-readable label for one binding, e.g. "W", "Space", "Enter (numpad)".
inline QString keyDisplayName(int encoded)
{
    if (encoded == 0) {
        return QStringLiteral("(none)");
    }
    QString name = QKeySequence(decodeQtKey(encoded)).toString();
    if (name.isEmpty()) {
        name = QStringLiteral("Key %1").arg(decodeQtKey(encoded));
    }
    if (decodeKeypad(encoded)) {
        name += QStringLiteral(" (numpad)");
    }
    return name;
}

/// Serialize to a compact comma-separated string for QSettings / profile JSON.
inline QString encodeMapToString(const KeyboardJoystickMap& m)
{
    return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9")
        .arg(m.up).arg(m.down).arg(m.left).arg(m.right)
        .arg(m.ul).arg(m.ur).arg(m.ll).arg(m.lr).arg(m.fire);
}

/// Parse a string produced by encodeMapToString(). Returns false if malformed.
inline bool decodeMapFromString(const QString& s, KeyboardJoystickMap& out)
{
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() != 9) {
        return false;
    }
    int values[9];
    for (int i = 0; i < 9; ++i) {
        bool ok = false;
        values[i] = parts[i].toInt(&ok);
        if (!ok) {
            return false;
        }
    }
    // Fire and cardinals must be bound; diagonals may be 0.
    if (values[0] == 0 || values[1] == 0 || values[2] == 0 || values[3] == 0 || values[8] == 0) {
        return false;
    }
    out.up = values[0];
    out.down = values[1];
    out.left = values[2];
    out.right = values[3];
    out.ul = values[4];
    out.ur = values[5];
    out.ll = values[6];
    out.lr = values[7];
    out.fire = values[8];
    return true;
}

/// Identify which preset a map matches, or "custom".
inline QString presetNameForMap(const KeyboardJoystickMap& m)
{
    if (m == KeyboardJoystickMap::numpadPreset()) return QStringLiteral("numpad");
    if (m == KeyboardJoystickMap::arrowsPreset()) return QStringLiteral("arrows");
    if (m == KeyboardJoystickMap::wasdPreset()) return QStringLiteral("wasd");
    return QStringLiteral("custom");
}

} // namespace KbdJoy

#endif // KEYBOARDJOYSTICKMAP_H
