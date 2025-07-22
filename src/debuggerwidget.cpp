/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "debuggerwidget.h"
#include <QDebug>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollBar>

extern "C" {
    // Access to CPU registers
    extern unsigned short CPU_regPC;
    extern unsigned char CPU_regA;
    extern unsigned char CPU_regX;
    extern unsigned char CPU_regY;
    extern unsigned char CPU_regS;
    extern unsigned char CPU_regP;
    // Access to memory
    extern unsigned char MEMORY_mem[65536];
}

DebuggerWidget::DebuggerWidget(AtariEmulator* emulator, QWidget *parent)
    : QWidget(parent)
    , m_emulator(emulator)
    , m_refreshTimer(new QTimer(this))
    , m_isRunning(false)
    , m_currentMemoryAddress(0x0000)
{
    setupUI();
    connectSignals();
    
    // Set up refresh timer (update every 100ms when running)
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &DebuggerWidget::refreshDebugInfo);
    
    // Initial update
    updateCPUState();
    updateMemoryView();
}

void DebuggerWidget::setupUI()
{
    setWindowTitle("Atari Debugger");
    setMinimumWidth(300);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    
    // CPU Registers Group
    m_cpuGroup = new QGroupBox("CPU Registers");
    QVBoxLayout* cpuMainLayout = new QVBoxLayout(m_cpuGroup);
    
    m_regALabel = new QLabel("$00");
    m_regXLabel = new QLabel("$00");
    m_regYLabel = new QLabel("$00");
    m_regPCLabel = new QLabel("$0000");
    m_regSPLabel = new QLabel("$00");
    m_regPLabel = new QLabel("$00");
    
    // Use monospace font for registers
    QFont monoFont("Courier", 10);
    m_regALabel->setFont(monoFont);
    m_regXLabel->setFont(monoFont);
    m_regYLabel->setFont(monoFont);
    m_regPCLabel->setFont(monoFont);
    m_regSPLabel->setFont(monoFont);
    m_regPLabel->setFont(monoFont);
    
    // First row: A, X, Y
    QHBoxLayout* row1Layout = new QHBoxLayout();
    row1Layout->addWidget(new QLabel("A:"));
    row1Layout->addWidget(m_regALabel);
    row1Layout->addWidget(new QLabel("X:"));
    row1Layout->addWidget(m_regXLabel);
    row1Layout->addWidget(new QLabel("Y:"));
    row1Layout->addWidget(m_regYLabel);
    row1Layout->addStretch();
    
    // Second row: PC, SP, P
    QHBoxLayout* row2Layout = new QHBoxLayout();
    row2Layout->addWidget(new QLabel("PC:"));
    row2Layout->addWidget(m_regPCLabel);
    row2Layout->addWidget(new QLabel("SP:"));
    row2Layout->addWidget(m_regSPLabel);
    row2Layout->addWidget(new QLabel("P:"));
    row2Layout->addWidget(m_regPLabel);
    row2Layout->addStretch();
    
    cpuMainLayout->addLayout(row1Layout);
    cpuMainLayout->addLayout(row2Layout);
    
    mainLayout->addWidget(m_cpuGroup);
    
    // Control Buttons Group
    m_controlGroup = new QGroupBox("Debug Controls");
    QVBoxLayout* controlLayout = new QVBoxLayout(m_controlGroup);
    
    QHBoxLayout* buttonRow1 = new QHBoxLayout();
    m_stepButton = new QPushButton("Step");
    m_stepButton->setToolTip("Execute one instruction");
    m_runButton = new QPushButton("Run");
    m_runButton->setToolTip("Continue execution");
    
    buttonRow1->addWidget(m_stepButton);
    buttonRow1->addWidget(m_runButton);
    controlLayout->addLayout(buttonRow1);
    
    QHBoxLayout* buttonRow2 = new QHBoxLayout();
    m_pauseButton = new QPushButton("Pause");
    m_pauseButton->setToolTip("Pause execution");
    m_pauseButton->setEnabled(false);
    m_refreshButton = new QPushButton("Refresh");
    m_refreshButton->setToolTip("Update debug information");
    
    buttonRow2->addWidget(m_pauseButton);
    buttonRow2->addWidget(m_refreshButton);
    controlLayout->addLayout(buttonRow2);
    
    mainLayout->addWidget(m_controlGroup);
    
    // Memory Viewer Group
    m_memoryGroup = new QGroupBox("Memory Viewer");
    QVBoxLayout* memoryLayout = new QVBoxLayout(m_memoryGroup);
    
    // Memory address input
    QHBoxLayout* addressLayout = new QHBoxLayout();
    addressLayout->addWidget(new QLabel("Address:"));
    
    m_memoryAddressSpinBox = new QSpinBox();
    m_memoryAddressSpinBox->setRange(0x0000, 0xFFFF);
    m_memoryAddressSpinBox->setDisplayIntegerBase(16);
    m_memoryAddressSpinBox->setPrefix("$");
    m_memoryAddressSpinBox->setValue(0x0000);
    m_memoryAddressSpinBox->setMinimumWidth(80);  // Make address input wider
    m_memoryAddressSpinBox->setFont(QFont("Courier", 10));  // Use monospace font
    addressLayout->addWidget(m_memoryAddressSpinBox);
    addressLayout->addStretch();
    
    memoryLayout->addLayout(addressLayout);
    
    // Memory display - remove height restriction and let it expand
    m_memoryTextEdit = new QTextEdit();
    m_memoryTextEdit->setFont(QFont("Courier", 9));
    m_memoryTextEdit->setReadOnly(true);
    m_memoryTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_memoryTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_memoryTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    memoryLayout->addWidget(m_memoryTextEdit);
    
    mainLayout->addWidget(m_memoryGroup);
}

void DebuggerWidget::connectSignals()
{
    connect(m_stepButton, &QPushButton::clicked, this, &DebuggerWidget::onStepClicked);
    connect(m_runButton, &QPushButton::clicked, this, &DebuggerWidget::onRunClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &DebuggerWidget::onPauseClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &DebuggerWidget::onRefreshClicked);
    
    connect(m_memoryAddressSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DebuggerWidget::onMemoryAddressChanged);
}

void DebuggerWidget::updateCPUState()
{
    updateCPURegisters();
}

void DebuggerWidget::updateMemoryView()
{
    updateMemoryDisplay();
}

void DebuggerWidget::updateCPURegisters()
{
    if (!m_emulator) {
        return;
    }
    
    // Update CPU register displays
    m_regALabel->setText(formatHexByte(CPU_regA));
    m_regXLabel->setText(formatHexByte(CPU_regX));
    m_regYLabel->setText(formatHexByte(CPU_regY));
    m_regPCLabel->setText(formatHexWord(CPU_regPC));
    m_regSPLabel->setText(formatHexByte(CPU_regS));
    m_regPLabel->setText(formatHexByte(CPU_regP));
}

void DebuggerWidget::updateMemoryDisplay()
{
    if (!m_emulator) {
        return;
    }
    
    m_currentMemoryAddress = m_memoryAddressSpinBox->value();
    
    QString memoryText;
    memoryText.reserve(2048); // Pre-allocate for efficiency
    
    // Display 16 rows of 16 bytes each
    for (int row = 0; row < MEMORY_DISPLAY_ROWS; row++) {
        int baseAddr = m_currentMemoryAddress + (row * MEMORY_DISPLAY_COLS);
        
        // Address column
        memoryText += QString("%1: ").arg(baseAddr, 4, 16, QChar('0')).toUpper();
        
        // Hex bytes
        QString hexPart;
        QString asciiPart;
        
        for (int col = 0; col < MEMORY_DISPLAY_COLS; col++) {
            int addr = baseAddr + col;
            if (addr <= 0xFFFF) {
                unsigned char byte = MEMORY_mem[addr];
                hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
                
                // ASCII representation (printable chars only)
                if (byte >= 32 && byte <= 126) {
                    asciiPart += QChar(byte);
                } else {
                    asciiPart += '.';
                }
            } else {
                hexPart += "   ";
                asciiPart += ' ';
            }
        }
        
        memoryText += hexPart + " | " + asciiPart + "\n";
    }
    
    m_memoryTextEdit->setText(memoryText);
}

QString DebuggerWidget::formatHexByte(unsigned char value)
{
    return QString("$%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString DebuggerWidget::formatHexWord(unsigned short value)
{
    return QString("$%1").arg(value, 4, 16, QChar('0')).toUpper();
}

void DebuggerWidget::onStepClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Step instruction";
    
    // TODO: Implement single step functionality
    // This will require integration with the emulator's step debugging
    
    // For now, just refresh the display
    refreshDebugInfo();
}

void DebuggerWidget::onRunClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Run/Continue execution";
    
    m_isRunning = true;
    m_stepButton->setEnabled(false);
    m_runButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    
    // Start refresh timer for live updates
    m_refreshTimer->start();
    
    // TODO: Resume emulator execution
}

void DebuggerWidget::onPauseClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Pause execution";
    
    m_isRunning = false;
    m_stepButton->setEnabled(true);
    m_runButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    
    // Stop refresh timer
    m_refreshTimer->stop();
    
    // Final update
    refreshDebugInfo();
    
    // TODO: Pause emulator execution
}

void DebuggerWidget::onMemoryAddressChanged()
{
    updateMemoryDisplay();
}

void DebuggerWidget::onRefreshClicked()
{
    refreshDebugInfo();
}

void DebuggerWidget::refreshDebugInfo()
{
    updateCPUState();
    updateMemoryView();
}