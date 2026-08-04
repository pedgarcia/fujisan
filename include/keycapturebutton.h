#ifndef KEYCAPTUREBUTTON_H
#define KEYCAPTUREBUTTON_H

#include <QPushButton>
#include <QPointer>

/// Button that captures a single key press when clicked.
///
/// Click to arm ("Press a key…"), the next key press becomes the binding
/// (encoded with KbdJoy::encodeKey, keeping the numpad distinction).
/// Escape cancels; Backspace/Delete clears the binding if clearable.
class KeyCaptureButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyCaptureButton(QWidget* parent = nullptr);

    void setEncodedKey(int encoded);
    int encodedKey() const { return m_encoded; }

    /// Allow clearing the binding (Backspace/Delete) — used for optional
    /// diagonal bindings. Required bindings ignore clear requests.
    void setClearable(bool clearable) { m_clearable = clearable; }

signals:
    void keyCaptured(int encoded);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void beginCapture();
    void endCapture();
    void updateLabel();

    bool m_capturing = false;
    bool m_clearable = false;
    int m_encoded = 0;

    // Only one button captures at a time (the keyboard grab is exclusive).
    static QPointer<KeyCaptureButton> s_activeCapture;
};

#endif // KEYCAPTUREBUTTON_H
