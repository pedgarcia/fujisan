#include "keycapturebutton.h"

#include <QKeyEvent>
#include <QFocusEvent>

#include "keyboardjoystickmap.h"

QPointer<KeyCaptureButton> KeyCaptureButton::s_activeCapture = nullptr;

KeyCaptureButton::KeyCaptureButton(QWidget* parent)
    : QPushButton(parent)
{
    setMinimumWidth(120);
    // macOS gives push buttons tab-only focus by default; we need keyboard focus
    // on click so the armed button actually receives the keystroke.
    setFocusPolicy(Qt::StrongFocus);
    connect(this, &QPushButton::clicked, this, &KeyCaptureButton::beginCapture);
    updateLabel();
}

void KeyCaptureButton::setEncodedKey(int encoded)
{
    m_encoded = encoded;
    updateLabel();
}

void KeyCaptureButton::beginCapture()
{
    // Disarm any other button that is still capturing (the grab transfers
    // silently, so without this it would stay stuck on "Press a key...").
    if (s_activeCapture && s_activeCapture != this) {
        s_activeCapture->endCapture();
    }
    s_activeCapture = this;

    m_capturing = true;
    setText(tr("Press a key..."));
    // Grab the keyboard so the next keystroke reaches this button no matter
    // where the window focus currently is.
    setFocus(Qt::OtherFocusReason);
    grabKeyboard();
}

void KeyCaptureButton::endCapture()
{
    if (s_activeCapture == this) {
        s_activeCapture = nullptr;
    }
    m_capturing = false;
    releaseKeyboard();
    updateLabel();
}

void KeyCaptureButton::updateLabel()
{
    if (m_capturing) {
        return;
    }
    setText(KbdJoy::keyDisplayName(m_encoded));
}

void KeyCaptureButton::keyPressEvent(QKeyEvent* event)
{
    if (!m_capturing) {
        QPushButton::keyPressEvent(event);
        return;
    }

    const int key = event->key();

    if (key == Qt::Key_Escape) {
        endCapture();
        event->accept();
        return;
    }

    if ((key == Qt::Key_Backspace || key == Qt::Key_Delete) && m_clearable) {
        m_encoded = 0;
        endCapture();
        emit keyCaptured(0);
        event->accept();
        return;
    }

    // Ignore pure modifier presses — wait for a real key.
    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt
        || key == Qt::Key_Meta || key == Qt::Key_AltGr || key == Qt::Key_unknown) {
        event->accept();
        return;
    }

    // Space/Return would normally re-trigger the button; while capturing they are bindings.
    m_encoded = KbdJoy::encodeKey(key, (event->modifiers() & Qt::KeypadModifier) != 0);
    endCapture();
    emit keyCaptured(m_encoded);
    event->accept();
}

void KeyCaptureButton::focusOutEvent(QFocusEvent* event)
{
    if (m_capturing) {
        endCapture();
    }
    QPushButton::focusOutEvent(event);
}
