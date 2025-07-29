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

class PrinterWidget : public QWidget
{
    Q_OBJECT

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

signals:
    void printerEnabledChanged(bool enabled);
    void outputFormatChanged(const QString& format);
    void printerTypeChanged(const QString& type);
    void printerCommandChanged(const QString& command);

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
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QCheckBox* m_enabledCheck;
    QLabel* m_statusLabel;
    
    // Output format section
    QHBoxLayout* m_formatLayout;
    QLabel* m_formatLabel;
    QComboBox* m_formatCombo;
    QComboBox* m_typeCombo;
    
    // Control buttons
    QHBoxLayout* m_controlsLayout;
    QPushButton* m_viewButton;
    QPushButton* m_saveButton;
    QPushButton* m_clearButton;
    QPushButton* m_testButton;
    
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
    
    // Constants
    static const QStringList OUTPUT_FORMATS;
    static const QString DEFAULT_FORMAT;
    static const QStringList PRINTER_TYPES;
    static const QString DEFAULT_PRINTER_TYPE;
    static const int MAX_BUFFER_SIZE;
};

#endif // PRINTERWIDGET_H