/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef PRINTERWIDGET_H
#define PRINTERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QPixmap>

class PrinterWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int formYOffset READ formYOffset WRITE setFormYOffset)
    Q_PROPERTY(int formYPos READ formYPos WRITE setFormYPos)

public:
    explicit PrinterWidget(QWidget *parent = nullptr);
    
    // Configuration
    void setPrinterEnabled(bool enabled);
    bool isPrinterEnabled() const { return m_printerEnabled; }
    
    void setOutputFormat(const QString& format);
    QString getOutputFormat() const;
    
    void setPrinterType(const QString& type);
    QString getPrinterType() const;
    
    // Text capture
    void appendText(const QString& text);
    void clearOutput();
    QString getOutputText() const;
    
    // File operations
    void saveToFile();
    void loadSettings();
    void saveSettings();

    // FujiNet printer output handling
    void displayPrinterOutput(const QByteArray& data, const QString& contentType);
    void showPrintingActivity();

signals:
    void printerEnabledChanged(bool enabled);
    void outputFormatChanged(const QString& format);
    void printerTypeChanged(const QString& type);
    void printerCommandChanged(const QString& command);
    void requestPausePrinterPolling();
    void requestResumePrinterPolling();
    void requestPrinterReconfigure();  // Request disable/enable cycle to reset server buffer

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onPrinterToggled(bool enabled);
    void onOutputFormatChanged(const QString& format);
    void onPrinterTypeChanged(const QString& type);
    void onViewOutputClicked();
    void onSaveOutputClicked();
    void onClearOutputClicked();
    void onPrintTestClicked();

private:
    void setupUI();
    void updatePrinterStatus();
    void updateControls();
    QString formatTextForOutput(const QString& rawText);
    void showOutputViewer();
    void updateViewerForCurrentState();
    void savePrinterOutput(const QByteArray& data, const QString& extension);
    bool validatePrinterOutput(const QByteArray& data, const QString& contentType);

    // Animation and form display methods
    int formYOffset() const { return m_formYOffset; }
    void setFormYOffset(int offset);
    int formYPos() const { return m_formYPos; }
    void setFormYPos(int pos);
    void updateFormDisplay();
    void scrollFormUp(int scrollAmount = 40);
    void expandFormDisplay(int additionalHeight);

    // UI Components
    QVBoxLayout* m_mainLayout;
    QCheckBox* m_enabledCheck;
    QLabel* m_statusLabel;
    
    // Output format section
    QHBoxLayout* m_formatLayout;
    QLabel* m_formatLabel;
    QComboBox* m_formatCombo;
    QComboBox* m_typeCombo;
    
    // Form display
    QLabel* m_formDisplay;
    QPixmap m_formPixmap;

    // Control buttons
    QHBoxLayout* m_controlsLayout;
    QPushButton* m_saveButton;
    QPushButton* m_tearButton;  // Renamed from m_clearButton
    
    // Output viewer (popup)
    QWidget* m_outputViewer;
    QPlainTextEdit* m_outputDisplay;
    QLabel* m_outputViewerInfo;
    
    // Internal state
    bool m_printerEnabled;
    QString m_outputFormat;
    QString m_printerType;
    QString m_outputBuffer;
    QString m_lastSaveDir;
    QString m_lastSavedPath;  // Track last saved file for opening
    QByteArray m_lastPrinterData;  // Last binary data received from FujiNet
    QString m_lastContentType;     // Content type of last received data
    QByteArray m_lastReceivedHash; // MD5 hash of last received data for deduplication
    qint64 m_lastProcessedTime;    // Timestamp when data was last processed
    int m_duplicateCount;          // Track consecutive duplicate receives

    // Animation infrastructure
    QPropertyAnimation* m_scrollAnimation;  // Scroll up animation
    QPropertyAnimation* m_tearAnimation;    // Tear effect animation
    QPropertyAnimation* m_slideAnimation;   // Slide up from bottom animation
    int m_formYOffset;                      // Current scroll position for animation
    int m_formYPos;                         // Y position for slide animation
    int m_maxDisplayHeight;                 // Current display height (grows during printing)

    // Constants
    static const int FORM_TIP_HEIGHT = 50;   // Initial tip size
    static const int FORM_MAX_HEIGHT = 200;  // Max expansion when printing
    static const int MAX_DUPLICATES_BEFORE_PAUSE = 3;
    static const QStringList OUTPUT_FORMATS;
    static const QString DEFAULT_FORMAT;
    static const QStringList PRINTER_TYPES;
    static const QString DEFAULT_PRINTER_TYPE;
    static const int MAX_BUFFER_SIZE;
};

#endif // PRINTERWIDGET_H