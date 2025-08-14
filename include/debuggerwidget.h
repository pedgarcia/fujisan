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
#include <QListWidget>
#include <QSet>
#include "atariemulator.h"

class DebuggerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DebuggerWidget(AtariEmulator* emulator, QWidget *parent = nullptr);
    
    void updateCPUState();
    void updateMemoryView();
    void updateDisassemblyView();
    
    // Public breakpoint management for TCP server
    void addBreakpoint(unsigned short address);
    void removeBreakpoint(unsigned short address);
    void clearAllBreakpoints();
    
public slots:
    void onStepIntoClicked();
    void onStepOverClicked(); 
    void onRunClicked();
    void onPauseClicked();
    void onMemoryAddressChanged();
    void onAddBreakpointClicked();
    void onRemoveBreakpointClicked();
    void onBreakpointSelectionChanged();

private slots:
    void refreshDebugInfo();
    
    // Emulator signal handlers
    void onEmulatorBreakpointHit(unsigned short address);
    void onEmulatorBreakpointAdded(unsigned short address);
    void onEmulatorBreakpointRemoved(unsigned short address);
    void onEmulatorBreakpointsCleared();
    void onEmulatorExecutionPaused();
    void onEmulatorExecutionResumed();
    void onEmulatorDebugStepped();

private:
    void setupUI();
    void connectSignals();
    void updateCPURegisters();
    void updateMemoryDisplay();
    void updateCurrentInstruction();
    void updateDisassemblyDisplay();
    QString formatHexByte(unsigned char value);
    QString formatHexWord(unsigned short value);
    QString formatCurrentInstruction(unsigned short pc);
    QString formatInstructionLine(unsigned short pc, bool isCurrent = false);
    int getInstructionSize(unsigned char opcode);
    bool isSubroutineCall(unsigned char opcode);
    void stepSingleInstruction();
    void stepOverSubroutine();
    
    // Breakpoint management
    bool hasBreakpoint(unsigned short address) const;
    void updateBreakpointList();
    void saveBreakpoints();
    void loadBreakpoints();
    void checkBreakpoints();
    
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
    QPushButton* m_stepIntoButton;
    QPushButton* m_stepOverButton;
    QPushButton* m_runButton;
    QPushButton* m_pauseButton;
    
    // Memory Viewer UI
    QGroupBox* m_memoryGroup;
    QSpinBox* m_memoryAddressSpinBox;
    QTextEdit* m_memoryTextEdit;
    
    // Disassembly UI
    QGroupBox* m_disassemblyGroup;
    QLabel* m_currentInstructionLabel;
    QTextEdit* m_disassemblyTextEdit;
    
    // Breakpoint UI
    QGroupBox* m_breakpointGroup;
    QSpinBox* m_breakpointAddressSpinBox;
    QPushButton* m_addBreakpointButton;
    QPushButton* m_removeBreakpointButton;
    QPushButton* m_clearBreakpointsButton;
    QListWidget* m_breakpointListWidget;
    
    // State tracking
    bool m_isRunning;
    int m_currentMemoryAddress;
    unsigned short m_stepOverBreakPC;  // PC to break at for step over
    
    // Breakpoint data
    QSet<unsigned short> m_breakpoints;
    unsigned short m_lastPC;  // Track PC changes for breakpoint detection
    
    static const int MEMORY_DISPLAY_ROWS = 20;  // Increased to 20 for more memory context
    static const int MEMORY_DISPLAY_COLS = 16;
};

#endif // DEBUGGERWIDGET_H