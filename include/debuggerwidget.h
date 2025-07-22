/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef DEBUGGERWIDGET_H
#define DEBUGGERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QFont>
#include <QTimer>
#include <QSpinBox>
#include "atariemulator.h"

class DebuggerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DebuggerWidget(AtariEmulator* emulator, QWidget *parent = nullptr);
    
    void updateCPUState();
    void updateMemoryView();
    
public slots:
    void onStepClicked();
    void onRunClicked();
    void onPauseClicked();
    void onMemoryAddressChanged();
    void onRefreshClicked();

private slots:
    void refreshDebugInfo();

private:
    void setupUI();
    void connectSignals();
    void updateCPURegisters();
    void updateMemoryDisplay();
    QString formatHexByte(unsigned char value);
    QString formatHexWord(unsigned short value);
    
    AtariEmulator* m_emulator;
    QTimer* m_refreshTimer;
    
    // CPU State UI
    QGroupBox* m_cpuGroup;
    QLabel* m_regALabel;
    QLabel* m_regXLabel;
    QLabel* m_regYLabel;
    QLabel* m_regPCLabel;
    QLabel* m_regSPLabel;
    QLabel* m_regPLabel;
    
    // Step Controls UI
    QGroupBox* m_controlGroup;
    QPushButton* m_stepButton;
    QPushButton* m_runButton;
    QPushButton* m_pauseButton;
    QPushButton* m_refreshButton;
    
    // Memory Viewer UI
    QGroupBox* m_memoryGroup;
    QSpinBox* m_memoryAddressSpinBox;
    QTextEdit* m_memoryTextEdit;
    
    // State tracking
    bool m_isRunning;
    int m_currentMemoryAddress;
    
    static const int MEMORY_DISPLAY_ROWS = 16;
    static const int MEMORY_DISPLAY_COLS = 16;
};

#endif // DEBUGGERWIDGET_H