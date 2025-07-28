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
#include <QCoreApplication>
#include <QFontMetrics>
#include <QSettings>
#include <algorithm>

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
    , m_isRunning(true)  // Start in running state to match emulator
    , m_currentMemoryAddress(0x0000)
    , m_lastPC(0x0000)
{
    setupUI();
    connectSignals();
    
    // Set up refresh timer (update every 100ms when running)
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &DebuggerWidget::refreshDebugInfo);
    
    // Start in running state to match the emulator
    if (m_stepIntoButton && m_stepOverButton && m_runButton && m_pauseButton) {
        m_stepIntoButton->setEnabled(false);
        m_stepOverButton->setEnabled(false);
        m_runButton->setEnabled(false);
        m_pauseButton->setEnabled(true);
        m_refreshTimer->start();
    }
    
    // Load saved breakpoints
    loadBreakpoints();
    
    // Initial update
    updateCPUState();
    updateMemoryView();
    updateDisassemblyView();
}

void DebuggerWidget::setupUI()
{
    setWindowTitle("Atari Debugger");
    setMinimumWidth(473);  // Increased from 300 to 473 (55% wider)
    
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
    m_stepIntoButton = new QPushButton("Step Into");
    m_stepIntoButton->setToolTip("Execute one instruction (F11)");
    m_stepIntoButton->setShortcut(QKeySequence(Qt::Key_F11));
    m_stepOverButton = new QPushButton("Step Over");
    m_stepOverButton->setToolTip("Step over subroutine calls (F10)");
    m_stepOverButton->setShortcut(QKeySequence(Qt::Key_F10));
    
    buttonRow1->addWidget(m_stepIntoButton);
    buttonRow1->addWidget(m_stepOverButton);
    controlLayout->addLayout(buttonRow1);
    
    QHBoxLayout* buttonRow2 = new QHBoxLayout();
    m_runButton = new QPushButton("Run");
    m_runButton->setToolTip("Continue execution (F5)");
    m_runButton->setShortcut(QKeySequence(Qt::Key_F5));
    m_pauseButton = new QPushButton("Pause");
    m_pauseButton->setToolTip("Pause execution");
    m_pauseButton->setEnabled(false);
    
    buttonRow2->addWidget(m_runButton);
    buttonRow2->addWidget(m_pauseButton);
    controlLayout->addLayout(buttonRow2);
    
    
    mainLayout->addWidget(m_controlGroup);
    
    // Breakpoint Management Group
    m_breakpointGroup = new QGroupBox("Breakpoints");
    QVBoxLayout* breakpointLayout = new QVBoxLayout(m_breakpointGroup);
    
    // Breakpoint address input and add/remove buttons
    QHBoxLayout* breakpointControlLayout = new QHBoxLayout();
    breakpointControlLayout->addWidget(new QLabel("Address:"));
    
    m_breakpointAddressSpinBox = new QSpinBox();
    m_breakpointAddressSpinBox->setRange(0x0000, 0xFFFF);
    m_breakpointAddressSpinBox->setDisplayIntegerBase(16);
    m_breakpointAddressSpinBox->setPrefix("$");
    m_breakpointAddressSpinBox->setValue(0x0000);
    m_breakpointAddressSpinBox->setMinimumWidth(80);
    m_breakpointAddressSpinBox->setFont(QFont("Courier", 10));
    m_breakpointAddressSpinBox->setToolTip("Enter address for new breakpoint");
    breakpointControlLayout->addWidget(m_breakpointAddressSpinBox);
    
    m_addBreakpointButton = new QPushButton("Add");
    m_addBreakpointButton->setToolTip("Add breakpoint at specified address (Ctrl+B)");
    m_addBreakpointButton->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
    m_addBreakpointButton->setMaximumWidth(60);
    breakpointControlLayout->addWidget(m_addBreakpointButton);
    
    m_removeBreakpointButton = new QPushButton("Remove");
    m_removeBreakpointButton->setToolTip("Remove selected breakpoint");
    m_removeBreakpointButton->setMaximumWidth(70);
    m_removeBreakpointButton->setEnabled(false);
    breakpointControlLayout->addWidget(m_removeBreakpointButton);
    
    m_clearBreakpointsButton = new QPushButton("Clear All");
    m_clearBreakpointsButton->setToolTip("Remove all breakpoints");
    m_clearBreakpointsButton->setMaximumWidth(80);
    breakpointControlLayout->addWidget(m_clearBreakpointsButton);
    
    breakpointLayout->addLayout(breakpointControlLayout);
    
    // Breakpoint list
    m_breakpointListWidget = new QListWidget();
    m_breakpointListWidget->setFont(QFont("Courier", 9));
    m_breakpointListWidget->setMaximumHeight(120);
    m_breakpointListWidget->setToolTip("Active breakpoints - execution will pause when PC reaches these addresses");
    breakpointLayout->addWidget(m_breakpointListWidget);
    
    mainLayout->addWidget(m_breakpointGroup);
    
    // Disassembly Group
    m_disassemblyGroup = new QGroupBox("Disassembly");
    QVBoxLayout* disassemblyLayout = new QVBoxLayout(m_disassemblyGroup);
    
    // Current instruction label (highlighted)
    m_currentInstructionLabel = new QLabel("$0000: ??? ???");
    m_currentInstructionLabel->setFont(QFont("Courier", 10));
    m_currentInstructionLabel->setStyleSheet("QLabel { background-color: #ffff99; padding: 4px; border: 1px solid #ccc; font-weight: bold; }");
    m_currentInstructionLabel->setAlignment(Qt::AlignLeft);
    
    disassemblyLayout->addWidget(m_currentInstructionLabel);
    
    // Disassembly text display
    m_disassemblyTextEdit = new QTextEdit();
    m_disassemblyTextEdit->setFont(QFont("Courier", 9));
    m_disassemblyTextEdit->setReadOnly(true);
    m_disassemblyTextEdit->setMaximumHeight(400);  // Increased height for scrollable content
    m_disassemblyTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);  // Always show scrollbar
    m_disassemblyTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    disassemblyLayout->addWidget(m_disassemblyTextEdit);
    mainLayout->addWidget(m_disassemblyGroup);
    
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
    connect(m_stepIntoButton, &QPushButton::clicked, this, &DebuggerWidget::onStepIntoClicked);
    connect(m_stepOverButton, &QPushButton::clicked, this, &DebuggerWidget::onStepOverClicked);
    connect(m_runButton, &QPushButton::clicked, this, &DebuggerWidget::onRunClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &DebuggerWidget::onPauseClicked);
    
    connect(m_memoryAddressSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DebuggerWidget::onMemoryAddressChanged);
    
    // Breakpoint controls
    connect(m_addBreakpointButton, &QPushButton::clicked, this, &DebuggerWidget::onAddBreakpointClicked);
    connect(m_removeBreakpointButton, &QPushButton::clicked, this, &DebuggerWidget::onRemoveBreakpointClicked);
    connect(m_clearBreakpointsButton, &QPushButton::clicked, this, &DebuggerWidget::clearAllBreakpoints);
    connect(m_breakpointListWidget, &QListWidget::itemSelectionChanged, this, &DebuggerWidget::onBreakpointSelectionChanged);
}

void DebuggerWidget::updateCPUState()
{
    updateCPURegisters();
}

void DebuggerWidget::updateMemoryView()
{
    updateMemoryDisplay();
}

void DebuggerWidget::updateDisassemblyView()
{
    updateCurrentInstruction();
    updateDisassemblyDisplay();
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

void DebuggerWidget::updateCurrentInstruction()
{
    if (!m_emulator) {
        m_currentInstructionLabel->setText("$0000: ??? ???");
        return;
    }
    
    QString instruction = formatCurrentInstruction(CPU_regPC);
    m_currentInstructionLabel->setText(instruction);
}

QString DebuggerWidget::formatCurrentInstruction(unsigned short pc)
{
    // Simple 6502 instruction formatting
    unsigned char opcode = MEMORY_mem[pc];
    
    // Basic opcode to mnemonic mapping (simplified)
    QString mnemonic = "???";
    QString operand = "";
    
    // Simple instruction decode - just a few common opcodes for demonstration
    switch (opcode) {
        case 0x00: mnemonic = "BRK"; break;
        case 0x10: mnemonic = "BPL"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0x20: {
            unsigned short addr = MEMORY_mem[(pc + 1) & 0xFFFF] | (MEMORY_mem[(pc + 2) & 0xFFFF] << 8);
            mnemonic = "JSR"; 
            operand = QString(" $%1").arg(addr, 4, 16, QChar('0')).toUpper();
            break;
        }
        case 0x30: mnemonic = "BMI"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0x40: mnemonic = "RTI"; break;
        case 0x50: mnemonic = "BVC"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0x60: mnemonic = "RTS"; break;
        case 0x70: mnemonic = "BVS"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0x90: mnemonic = "BCC"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xa0: mnemonic = "LDY"; operand = QString(" #$%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xa2: mnemonic = "LDX"; operand = QString(" #$%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xa9: mnemonic = "LDA"; operand = QString(" #$%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xb0: mnemonic = "BCS"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xd0: mnemonic = "BNE"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        case 0xea: mnemonic = "NOP"; break;
        case 0xf0: mnemonic = "BEQ"; operand = QString(" $%1").arg(MEMORY_mem[(pc + 1) & 0xFFFF], 2, 16, QChar('0')).toUpper(); break;
        default:
            mnemonic = "???";
            operand = QString(" $%1").arg(opcode, 2, 16, QChar('0')).toUpper();
            break;
    }
    
    return QString("$%1: %2 %3%4")
        .arg(pc, 4, 16, QChar('0')).toUpper()
        .arg(opcode, 2, 16, QChar('0')).toUpper()
        .arg(mnemonic)
        .arg(operand);
}

void DebuggerWidget::updateDisassemblyDisplay()
{
    if (!m_emulator) {
        m_disassemblyTextEdit->clear();
        return;
    }
    
    QString disassemblyText;
    disassemblyText.reserve(2048);
    
    unsigned short currentPC = CPU_regPC;
    unsigned short startPC = currentPC;
    
    // When paused, calculate start PC to center the current PC in the view
    int backtrack;
    if (m_isRunning) {
        backtrack = 16;  // Show less context when running for performance
    } else {
        // When paused, go back enough to center the current PC
        const int maxInstructions = 100;
        backtrack = maxInstructions / 2;  // Center the PC in the middle of our instruction window
    }
    
    if (startPC >= backtrack) {
        startPC -= backtrack;
    } else {
        startPC = 0;
    }
    
    // Show more instructions when paused for scrolling, fewer when running for performance
    unsigned short pc = startPC;
    int instructionCount = 0;
    const int maxInstructions = m_isRunning ? 20 : 100;  // 100 instructions when paused, 20 when running
    
    while (instructionCount < maxInstructions && pc < 0xFFFF) {
        QString line = formatInstructionLine(pc, pc == currentPC);
        disassemblyText += line + "\n";
        
        // Advance PC by instruction size
        unsigned char opcode = MEMORY_mem[pc];
        pc += getInstructionSize(opcode);
        instructionCount++;
        
        // Stop if we've gone too far past current PC (more leeway when paused)
        int maxForward = m_isRunning ? 20 : 80;
        if (pc > currentPC + maxForward) {
            break;
        }
    }
    
    // Use plain text formatting
    m_disassemblyTextEdit->setText(disassemblyText);
    
    // When paused, scroll to center the PC line (marked with "->")
    if (!m_isRunning) {
        // Find the PC line (marked with "->") in the text
        QStringList lines = disassemblyText.split('\n');
        int currentPCLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].startsWith("->")) {
                currentPCLine = i;
                break;
            }
        }
        
        if (currentPCLine >= 0) {
            // Use direct scrollbar manipulation to center the PC line
            QScrollBar* scrollBar = m_disassemblyTextEdit->verticalScrollBar();
            int lineHeight = QFontMetrics(m_disassemblyTextEdit->font()).lineSpacing();
            int visibleLines = m_disassemblyTextEdit->viewport()->height() / lineHeight;
            
            // Calculate scroll position to center the PC line
            int targetLine = qMax(0, currentPCLine - visibleLines / 2);
            int scrollValue = targetLine * lineHeight;
            
            qDebug() << QString("Centering PC: currentPCLine=%1, visibleLines=%2, targetLine=%3, scrollValue=%4")
                        .arg(currentPCLine).arg(visibleLines).arg(targetLine).arg(scrollValue);
            
            scrollBar->setValue(scrollValue);
        }
    }
}

QString DebuggerWidget::formatInstructionLine(unsigned short pc, bool isCurrent)
{
    unsigned char opcode = MEMORY_mem[pc];
    
    // Basic opcode to mnemonic mapping (expanded from current instruction formatter)
    QString mnemonic = "???";
    QString operand = "";
    QString hexBytes = QString("%1").arg(opcode, 2, 16, QChar('0')).toUpper();
    
    // Instruction decode with proper operand handling
    switch (opcode) {
        // Implied instructions (1 byte)
        case 0x00: mnemonic = "BRK"; break;
        case 0x08: mnemonic = "PHP"; break;
        case 0x18: mnemonic = "CLC"; break;
        case 0x28: mnemonic = "PLP"; break;
        case 0x38: mnemonic = "SEC"; break;
        case 0x40: mnemonic = "RTI"; break;
        case 0x48: mnemonic = "PHA"; break;
        case 0x58: mnemonic = "CLI"; break;
        case 0x60: mnemonic = "RTS"; break;
        case 0x68: mnemonic = "PLA"; break;
        case 0x78: mnemonic = "SEI"; break;
        case 0x88: mnemonic = "DEY"; break;
        case 0x8a: mnemonic = "TXA"; break;
        case 0x98: mnemonic = "TYA"; break;
        case 0x9a: mnemonic = "TXS"; break;
        case 0xa8: mnemonic = "TAY"; break;
        case 0xaa: mnemonic = "TAX"; break;
        case 0xb8: mnemonic = "CLV"; break;
        case 0xba: mnemonic = "TSX"; break;
        case 0xc8: mnemonic = "INY"; break;
        case 0xca: mnemonic = "DEX"; break;
        case 0xd8: mnemonic = "CLD"; break;
        case 0xe8: mnemonic = "INX"; break;
        case 0xea: mnemonic = "NOP"; break;
        case 0xf8: mnemonic = "SED"; break;
        
        // Relative branches (2 bytes)
        case 0x10: case 0x30: case 0x50: case 0x70: case 0x90: case 0xb0: case 0xd0: case 0xf0: {
            unsigned char operandByte = MEMORY_mem[(pc + 1) & 0xFFFF];
            hexBytes += QString(" %1").arg(operandByte, 2, 16, QChar('0')).toUpper();
            unsigned short target = (pc + 2 + (signed char)operandByte) & 0xFFFF;
            operand = QString(" $%1").arg(target, 4, 16, QChar('0')).toUpper();
            
            switch (opcode) {
                case 0x10: mnemonic = "BPL"; break;
                case 0x30: mnemonic = "BMI"; break;
                case 0x50: mnemonic = "BVC"; break;
                case 0x70: mnemonic = "BVS"; break;
                case 0x90: mnemonic = "BCC"; break;
                case 0xb0: mnemonic = "BCS"; break;
                case 0xd0: mnemonic = "BNE"; break;
                case 0xf0: mnemonic = "BEQ"; break;
            }
            break;
        }
        
        // Immediate instructions (2 bytes)
        case 0x09: case 0x29: case 0x49: case 0x69: case 0x89: case 0xa0: case 0xa2: case 0xa9:
        case 0xc0: case 0xc9: case 0xe0: case 0xe9: {
            unsigned char operandByte = MEMORY_mem[(pc + 1) & 0xFFFF];
            hexBytes += QString(" %1").arg(operandByte, 2, 16, QChar('0')).toUpper();
            operand = QString(" #$%1").arg(operandByte, 2, 16, QChar('0')).toUpper();
            
            switch (opcode) {
                case 0x09: mnemonic = "ORA"; break;
                case 0x29: mnemonic = "AND"; break;
                case 0x49: mnemonic = "EOR"; break;
                case 0x69: mnemonic = "ADC"; break;
                case 0x89: mnemonic = "STA"; break; // Illegal but common
                case 0xa0: mnemonic = "LDY"; break;
                case 0xa2: mnemonic = "LDX"; break;
                case 0xa9: mnemonic = "LDA"; break;
                case 0xc0: mnemonic = "CPY"; break;
                case 0xc9: mnemonic = "CMP"; break;
                case 0xe0: mnemonic = "CPX"; break;
                case 0xe9: mnemonic = "SBC"; break;
            }
            break;
        }
        
        // Zero page instructions (2 bytes)
        case 0x05: case 0x06: case 0x24: case 0x25: case 0x26: case 0x45: case 0x46: case 0x65: case 0x66:
        case 0x84: case 0x85: case 0x86: case 0xa4: case 0xa5: case 0xa6: case 0xc4: case 0xc5: case 0xc6:
        case 0xe4: case 0xe5: case 0xe6: {
            unsigned char operandByte = MEMORY_mem[(pc + 1) & 0xFFFF];
            hexBytes += QString(" %1").arg(operandByte, 2, 16, QChar('0')).toUpper();
            operand = QString(" $%1").arg(operandByte, 2, 16, QChar('0')).toUpper();
            
            switch (opcode) {
                case 0x05: mnemonic = "ORA"; break;
                case 0x06: mnemonic = "ASL"; break;
                case 0x24: mnemonic = "BIT"; break;
                case 0x25: mnemonic = "AND"; break;
                case 0x26: mnemonic = "ROL"; break;
                case 0x45: mnemonic = "EOR"; break;
                case 0x46: mnemonic = "LSR"; break;
                case 0x65: mnemonic = "ADC"; break;
                case 0x66: mnemonic = "ROR"; break;
                case 0x84: mnemonic = "STY"; break;
                case 0x85: mnemonic = "STA"; break;
                case 0x86: mnemonic = "STX"; break;
                case 0xa4: mnemonic = "LDY"; break;
                case 0xa5: mnemonic = "LDA"; break;
                case 0xa6: mnemonic = "LDX"; break;
                case 0xc4: mnemonic = "CPY"; break;
                case 0xc5: mnemonic = "CMP"; break;
                case 0xc6: mnemonic = "DEC"; break;
                case 0xe4: mnemonic = "CPX"; break;
                case 0xe5: mnemonic = "SBC"; break;
                case 0xe6: mnemonic = "INC"; break;
            }
            break;
        }
        
        // Absolute instructions (3 bytes)
        case 0x0d: case 0x0e: case 0x1d: case 0x1e: case 0x20: case 0x2c: case 0x2d: case 0x2e:
        case 0x3d: case 0x3e: case 0x4c: case 0x4d: case 0x4e: case 0x5d: case 0x5e: case 0x6c:
        case 0x6d: case 0x6e: case 0x7d: case 0x7e: case 0x8c: case 0x8d: case 0x8e: case 0x9d:
        case 0x9e: case 0xac: case 0xad: case 0xae: case 0xbc: case 0xbd: case 0xbe: case 0xcc:
        case 0xcd: case 0xce: case 0xdd: case 0xde: case 0xec: case 0xed: case 0xee: case 0xfd:
        case 0xfe: {
            unsigned char lo = MEMORY_mem[(pc + 1) & 0xFFFF];
            unsigned char hi = MEMORY_mem[(pc + 2) & 0xFFFF];
            unsigned short addr = lo | (hi << 8);
            hexBytes += QString(" %1 %2").arg(lo, 2, 16, QChar('0')).arg(hi, 2, 16, QChar('0')).toUpper();
            
            // Handle indexed addressing modes
            if ((opcode & 0x1F) == 0x1D) { // Absolute,X
                operand = QString(" $%1,X").arg(addr, 4, 16, QChar('0')).toUpper();
            } else if ((opcode & 0x1F) == 0x1E) { // Absolute,X
                operand = QString(" $%1,X").arg(addr, 4, 16, QChar('0')).toUpper();
            } else if (opcode == 0x9e) { // SHX abs,Y (undocumented)
                operand = QString(" $%1,Y").arg(addr, 4, 16, QChar('0')).toUpper();
            } else if ((opcode & 0x0F) == 0x09 && (opcode & 0xF0) >= 0xB0) { // Absolute,Y
                operand = QString(" $%1,Y").arg(addr, 4, 16, QChar('0')).toUpper();
            } else if (opcode == 0x6c) { // JMP (indirect)
                operand = QString(" ($%1)").arg(addr, 4, 16, QChar('0')).toUpper();
            } else {
                operand = QString(" $%1").arg(addr, 4, 16, QChar('0')).toUpper();
            }
            
            switch (opcode) {
                case 0x0d: mnemonic = "ORA"; break;
                case 0x0e: mnemonic = "ASL"; break;
                case 0x1d: mnemonic = "ORA"; break;
                case 0x1e: mnemonic = "ASL"; break;
                case 0x20: mnemonic = "JSR"; break;
                case 0x2c: mnemonic = "BIT"; break;
                case 0x2d: mnemonic = "AND"; break;
                case 0x2e: mnemonic = "ROL"; break;
                case 0x3d: mnemonic = "AND"; break;
                case 0x3e: mnemonic = "ROL"; break;
                case 0x4c: mnemonic = "JMP"; break;
                case 0x4d: mnemonic = "EOR"; break;
                case 0x4e: mnemonic = "LSR"; break;
                case 0x5d: mnemonic = "EOR"; break;
                case 0x5e: mnemonic = "LSR"; break;
                case 0x6c: mnemonic = "JMP"; break;
                case 0x6d: mnemonic = "ADC"; break;
                case 0x6e: mnemonic = "ROR"; break;
                case 0x7d: mnemonic = "ADC"; break;
                case 0x7e: mnemonic = "ROR"; break;
                case 0x8c: mnemonic = "STY"; break;
                case 0x8d: mnemonic = "STA"; break;
                case 0x8e: mnemonic = "STX"; break;
                case 0x9d: mnemonic = "STA"; break;
                case 0x9e: mnemonic = "SHX"; break; // Undocumented
                case 0xac: mnemonic = "LDY"; break;
                case 0xad: mnemonic = "LDA"; break;
                case 0xae: mnemonic = "LDX"; break;
                case 0xbc: mnemonic = "LDY"; break;
                case 0xbd: mnemonic = "LDA"; break;
                case 0xbe: mnemonic = "LDX"; break;
                case 0xcc: mnemonic = "CPY"; break;
                case 0xcd: mnemonic = "CMP"; break;
                case 0xce: mnemonic = "DEC"; break;
                case 0xdd: mnemonic = "CMP"; break;
                case 0xde: mnemonic = "DEC"; break;
                case 0xec: mnemonic = "CPX"; break;
                case 0xed: mnemonic = "SBC"; break;
                case 0xee: mnemonic = "INC"; break;
                case 0xfd: mnemonic = "SBC"; break;
                case 0xfe: mnemonic = "INC"; break;
            }
            break;
        }
        
        // Handle some common undocumented/illegal opcodes
        case 0xd2: mnemonic = "JAM"; operand = " *ILLEGAL*"; break; // KIL/JAM - processor halt
        case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52: case 0x62: case 0x72:
        case 0x92: case 0xb2: case 0xf2:
            mnemonic = "JAM"; operand = " *ILLEGAL*"; break; // More JAM opcodes
        
        default: {
            // Unknown instruction - show as illegal opcode
            mnemonic = "ILL";
            operand = QString(" *$%1*").arg(opcode, 2, 16, QChar('0')).toUpper();
            break;
        }
    }
    
    // Format the line with prominent arrow for current PC and breakpoint indicator
    QString prefix;
    if (isCurrent) {
        prefix = "-> ";  // Current PC
    } else if (hasBreakpoint(pc)) {
        prefix = "B  ";  // Breakpoint marker
    } else {
        prefix = "   ";  // Normal line
    }
    QString line = QString("%1%2: %3 %4%5")
        .arg(prefix)
        .arg(pc, 4, 16, QChar('0')).toUpper()
        .arg(hexBytes, -8)  // Left-align hex bytes in 8-char field
        .arg(mnemonic)
        .arg(operand);
    
    return line;
}

int DebuggerWidget::getInstructionSize(unsigned char opcode)
{
    // Return instruction size in bytes based on opcode
    // This is a simplified version - a complete implementation would use a lookup table
    
    switch (opcode) {
        // Implied instructions (1 byte)
        case 0x00: case 0x08: case 0x18: case 0x28: case 0x38: case 0x40:
        case 0x48: case 0x58: case 0x60: case 0x68: case 0x78: case 0x88:
        case 0x8a: case 0x98: case 0x9a: case 0xa8: case 0xaa: case 0xb8:
        case 0xba: case 0xc8: case 0xca: case 0xd8: case 0xe8: case 0xea:
        case 0xf8:
            return 1;
            
        // Immediate and relative instructions (2 bytes)
        case 0x05: case 0x06: case 0x09: case 0x10: case 0x24: case 0x25: case 0x26: case 0x29:
        case 0x30: case 0x45: case 0x46: case 0x49: case 0x50: case 0x65: case 0x66: case 0x69:
        case 0x70: case 0x84: case 0x85: case 0x86: case 0x89: case 0x90: case 0xa0: case 0xa2:
        case 0xa4: case 0xa5: case 0xa6: case 0xa9: case 0xb0: case 0xc0: case 0xc4: case 0xc5:
        case 0xc6: case 0xc9: case 0xd0: case 0xe0: case 0xe4: case 0xe5: case 0xe6: case 0xe9:
        case 0xf0:
            return 2;
            
        // Absolute instructions (3 bytes)  
        case 0x0d: case 0x0e: case 0x1d: case 0x1e: case 0x20: case 0x2c: case 0x2d: case 0x2e:
        case 0x3d: case 0x3e: case 0x4c: case 0x4d: case 0x4e: case 0x5d: case 0x5e: case 0x6c:
        case 0x6d: case 0x6e: case 0x7d: case 0x7e: case 0x8c: case 0x8d: case 0x8e: case 0x9d:
        case 0x9e: case 0xac: case 0xad: case 0xae: case 0xbc: case 0xbd: case 0xbe: case 0xcc:
        case 0xcd: case 0xce: case 0xdd: case 0xde: case 0xec: case 0xed: case 0xee: case 0xfd:
        case 0xfe:
            return 3;
            
        default:
            // Unknown instruction, assume 1 byte
            return 1;
    }
}

void DebuggerWidget::onStepIntoClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Step Into - execute single instruction";
    
    // Ensure emulator is paused before stepping
    if (!m_emulator->isEmulationPaused()) {
        m_emulator->pauseEmulation();
        m_isRunning = false;
        if (m_stepIntoButton && m_stepOverButton && m_runButton && m_pauseButton) {
            m_stepIntoButton->setEnabled(true);
            m_stepOverButton->setEnabled(true);
            m_runButton->setEnabled(true);
            m_pauseButton->setEnabled(false);
        }
        m_refreshTimer->stop();
    }
    
    // Execute single instruction
    stepSingleInstruction();
    
    // Update display immediately after step
    refreshDebugInfo();
}

void DebuggerWidget::onStepOverClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Step Over - step over subroutine calls";
    
    // Ensure emulator is paused before stepping
    if (!m_emulator->isEmulationPaused()) {
        m_emulator->pauseEmulation();
        m_isRunning = false;
        if (m_stepIntoButton && m_stepOverButton && m_runButton && m_pauseButton) {
            m_stepIntoButton->setEnabled(true);
            m_stepOverButton->setEnabled(true);
            m_runButton->setEnabled(true);
            m_pauseButton->setEnabled(false);
        }
        m_refreshTimer->stop();
    }
    
    // Step over subroutine calls
    stepOverSubroutine();
    
    // Update display immediately after step
    refreshDebugInfo();
}

void DebuggerWidget::onRunClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Run/Continue execution";
    
    // Resume emulator execution
    m_emulator->resumeEmulation();
    
    m_isRunning = true;
    if (m_stepIntoButton && m_stepOverButton && m_runButton && m_pauseButton) {
        m_stepIntoButton->setEnabled(false);
        m_stepOverButton->setEnabled(false);
        m_runButton->setEnabled(false);
        m_pauseButton->setEnabled(true);
    }
    
    // Start refresh timer for live updates
    m_refreshTimer->start();
}

void DebuggerWidget::onPauseClicked()
{
    if (!m_emulator) {
        return;
    }
    
    qDebug() << "Debug: Pause execution";
    
    // Pause emulator execution
    m_emulator->pauseEmulation();
    
    m_isRunning = false;
    if (m_stepIntoButton && m_stepOverButton && m_runButton && m_pauseButton) {
        m_stepIntoButton->setEnabled(true);
        m_stepOverButton->setEnabled(true);
        m_runButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
    }
    
    // Stop refresh timer
    m_refreshTimer->stop();
    
    // Final update
    refreshDebugInfo();
}

void DebuggerWidget::onMemoryAddressChanged()
{
    updateMemoryDisplay();
}


void DebuggerWidget::refreshDebugInfo()
{
    // Check breakpoints first (only when running)
    if (m_isRunning) {
        checkBreakpoints();
    }
    
    updateCPUState();
    updateMemoryView();
    updateDisassemblyView();
}

bool DebuggerWidget::isSubroutineCall(unsigned char opcode)
{
    // JSR (Jump to Subroutine) instruction
    return opcode == 0x20;
}

void DebuggerWidget::stepSingleInstruction()
{
    if (!m_emulator) {
        return;
    }
    
    // For instruction-level stepping, we need to execute until the PC changes
    // Since libatari800 only supports frame-level stepping, we'll use a simple approach:
    // Execute one frame and check if PC advanced by one instruction
    
    unsigned short startPC = CPU_regPC;
    unsigned char opcode = MEMORY_mem[startPC];
    int instructionSize = getInstructionSize(opcode);
    
    // Execute one frame (this is the limitation of libatari800)
    m_emulator->stepOneFrame();
    
    qDebug() << QString("Step Into: PC $%1 -> PC $%2 (expected size: %3)")
                .arg(startPC, 4, 16, QChar('0')).toUpper()
                .arg(CPU_regPC, 4, 16, QChar('0')).toUpper()
                .arg(instructionSize);
}

void DebuggerWidget::stepOverSubroutine()
{
    if (!m_emulator) {
        return;
    }
    
    unsigned short currentPC = CPU_regPC;
    unsigned char opcode = MEMORY_mem[currentPC];
    
    if (isSubroutineCall(opcode)) {
        // This is a JSR instruction - set breakpoint at return address
        m_stepOverBreakPC = currentPC + 3;  // JSR is 3 bytes
        
        qDebug() << QString("Step Over: JSR at $%1, setting break at $%2")
                    .arg(currentPC, 4, 16, QChar('0')).toUpper()
                    .arg(m_stepOverBreakPC, 4, 16, QChar('0')).toUpper();
        
        // Execute frames until we reach the breakpoint
        int maxFrames = 1000; // Safety limit
        int frameCount = 0;
        
        while (frameCount < maxFrames && CPU_regPC != m_stepOverBreakPC) {
            m_emulator->stepOneFrame();
            frameCount++;
            
            // Update display every 10 frames to show progress
            if (frameCount % 10 == 0) {
                refreshDebugInfo();
                QCoreApplication::processEvents(); // Allow UI updates
            }
        }
        
        if (frameCount >= maxFrames) {
            qDebug() << "Step Over: Timeout reached, subroutine may not return";
        } else {
            qDebug() << QString("Step Over: Completed after %1 frames, PC at $%2")
                        .arg(frameCount)
                        .arg(CPU_regPC, 4, 16, QChar('0')).toUpper();
        }
    } else {
        // Not a subroutine call, just step one instruction
        stepSingleInstruction();
    }
}

// Breakpoint Management Functions

void DebuggerWidget::onAddBreakpointClicked()
{
    unsigned short address = m_breakpointAddressSpinBox->value();
    addBreakpoint(address);
}

void DebuggerWidget::onRemoveBreakpointClicked()
{
    QListWidgetItem* selectedItem = m_breakpointListWidget->currentItem();
    if (selectedItem) {
        QString text = selectedItem->text();
        // Extract address from text (format: "$1234")
        bool ok;
        unsigned short address = text.mid(1).toUShort(&ok, 16);
        if (ok) {
            removeBreakpoint(address);
        }
    }
}

void DebuggerWidget::onBreakpointSelectionChanged()
{
    bool hasSelection = m_breakpointListWidget->currentItem() != nullptr;
    m_removeBreakpointButton->setEnabled(hasSelection);
}

void DebuggerWidget::addBreakpoint(unsigned short address)
{
    if (m_breakpoints.contains(address)) {
        qDebug() << QString("Breakpoint already exists at $%1").arg(address, 4, 16, QChar('0')).toUpper();
        return;
    }
    
    m_breakpoints.insert(address);
    updateBreakpointList();
    saveBreakpoints();
    
    qDebug() << QString("Added breakpoint at $%1").arg(address, 4, 16, QChar('0')).toUpper();
}

void DebuggerWidget::removeBreakpoint(unsigned short address)
{
    if (!m_breakpoints.contains(address)) {
        return;
    }
    
    m_breakpoints.remove(address);
    updateBreakpointList();
    saveBreakpoints();
    
    qDebug() << QString("Removed breakpoint at $%1").arg(address, 4, 16, QChar('0')).toUpper();
}

void DebuggerWidget::clearAllBreakpoints()
{
    if (m_breakpoints.isEmpty()) {
        return;
    }
    
    m_breakpoints.clear();
    updateBreakpointList();
    saveBreakpoints();
    
    qDebug() << "Cleared all breakpoints";
}

bool DebuggerWidget::hasBreakpoint(unsigned short address) const
{
    return m_breakpoints.contains(address);
}

void DebuggerWidget::updateBreakpointList()
{
    m_breakpointListWidget->clear();
    
    // Convert set to sorted list for display
    QList<unsigned short> sortedBreakpoints = m_breakpoints.values();
    std::sort(sortedBreakpoints.begin(), sortedBreakpoints.end());
    
    for (unsigned short address : sortedBreakpoints) {
        QString item = QString("$%1").arg(address, 4, 16, QChar('0')).toUpper();
        m_breakpointListWidget->addItem(item);
    }
    
    // Update remove button state
    m_removeBreakpointButton->setEnabled(false);
}

void DebuggerWidget::saveBreakpoints()
{
    QSettings settings;
    settings.beginGroup("debugger");
    
    // Convert breakpoints to string list
    QStringList breakpointList;
    for (unsigned short address : m_breakpoints) {
        breakpointList.append(QString::number(address));
    }
    
    settings.setValue("breakpoints", breakpointList);
    settings.endGroup();
}

void DebuggerWidget::loadBreakpoints()
{
    QSettings settings;
    settings.beginGroup("debugger");
    
    QStringList breakpointList = settings.value("breakpoints").toStringList();
    
    m_breakpoints.clear();
    for (const QString& addressStr : breakpointList) {
        bool ok;
        unsigned short address = addressStr.toUShort(&ok);
        if (ok) {
            m_breakpoints.insert(address);
        }
    }
    
    updateBreakpointList();
    settings.endGroup();
    
    if (!m_breakpoints.isEmpty()) {
        qDebug() << QString("Loaded %1 breakpoints from settings").arg(m_breakpoints.size());
    }
}

void DebuggerWidget::checkBreakpoints()
{
    if (m_breakpoints.isEmpty() || !m_emulator) {
        return;
    }
    
    unsigned short currentPC = CPU_regPC;
    
    // Only check if PC has changed to avoid repeated breaks
    if (currentPC != m_lastPC) {
        m_lastPC = currentPC;
        
        if (m_breakpoints.contains(currentPC)) {
            qDebug() << QString("BREAKPOINT HIT at $%1 - pausing execution").arg(currentPC, 4, 16, QChar('0')).toUpper();
            
            // Pause execution
            onPauseClicked();
        }
    }
}