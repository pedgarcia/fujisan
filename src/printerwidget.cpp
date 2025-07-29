/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "printerwidget.h"
#include <QSettings>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QDebug>

const QStringList PrinterWidget::OUTPUT_FORMATS = {"Text", "Raw"};
const QString PrinterWidget::DEFAULT_FORMAT = "Text";
const QStringList PrinterWidget::PRINTER_TYPES = {"Generic", "Atari 825", "Atari 820", "Atari 1020", "Atari 1025"};
const QString PrinterWidget::DEFAULT_PRINTER_TYPE = "Generic";
const int PrinterWidget::MAX_BUFFER_SIZE = 1000000; // 1MB limit

PrinterWidget::PrinterWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_enabledCheck(nullptr)
    , m_statusLabel(nullptr)
    , m_formatLayout(nullptr)
    , m_formatLabel(nullptr)
    , m_formatCombo(nullptr)
    , m_controlsLayout(nullptr)
    , m_viewButton(nullptr)
    , m_saveButton(nullptr)
    , m_clearButton(nullptr)
    , m_testButton(nullptr)
    , m_outputViewer(nullptr)
    , m_outputDisplay(nullptr)
    , m_outputViewerInfo(nullptr)
    , m_printerEnabled(false)
    , m_outputFormat(DEFAULT_FORMAT)
    , m_printerType(DEFAULT_PRINTER_TYPE)
    , m_lastSaveDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
{
    setupUI();
    loadSettings();
    updateControls();
}

void PrinterWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(4);

    // Enable/disable checkbox
    m_enabledCheck = new QCheckBox("Enable", this);
    m_enabledCheck->setToolTip("Enable/disable Atari P: device emulation");
    connect(m_enabledCheck, &QCheckBox::toggled, this, &PrinterWidget::onPrinterToggled);
    m_mainLayout->addWidget(m_enabledCheck);

    // Status label
    m_statusLabel = new QLabel("Status: Disabled", this);
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 10px; }");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_statusLabel);

    // Printer type selection
    QLabel* typeLabel = new QLabel("Type:", this);
    typeLabel->setStyleSheet("QLabel { font-size: 10px; }");
    m_mainLayout->addWidget(typeLabel);
    
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems(PRINTER_TYPES);
    m_typeCombo->setCurrentText(DEFAULT_PRINTER_TYPE);
    m_typeCombo->setMaximumHeight(24);
    m_typeCombo->setMinimumWidth(100); // Make combo box wider
    m_typeCombo->setToolTip("Select Atari printer type to emulate:\n"
                            "• Generic: Basic text printer\n"
                            "• Atari 825: 80-column dot matrix\n"
                            "• Atari 820: 40-column thermal\n"
                            "• Atari 1020: 4-color plotter\n"
                            "• Atari 1025: 80-column dot matrix");
    connect(m_typeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &PrinterWidget::onPrinterTypeChanged);
    m_mainLayout->addWidget(m_typeCombo, 0, Qt::AlignLeft);

    // Output format selection (vertical)
    QLabel* formatLabel = new QLabel("Format:", this);
    formatLabel->setStyleSheet("QLabel { font-size: 10px; }");
    m_mainLayout->addWidget(formatLabel);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems(OUTPUT_FORMATS);
    m_formatCombo->setCurrentText(DEFAULT_FORMAT);
    m_formatCombo->setMaximumHeight(24);
    m_formatCombo->setMinimumWidth(100); // Make combo box wider
    m_formatCombo->setToolTip("Select output format for printer data:\n"
                              "• Text: ATASCII converted to UTF-8\n"
                              "• Raw: Raw bytes as received");
    connect(m_formatCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &PrinterWidget::onOutputFormatChanged);
    m_mainLayout->addWidget(m_formatCombo, 0, Qt::AlignLeft);

    // Control buttons in vertical stack
    m_viewButton = new QPushButton("View", this);
    m_viewButton->setMinimumHeight(28);
    m_viewButton->setToolTip("View captured printer output");
    connect(m_viewButton, &QPushButton::clicked, this, &PrinterWidget::onViewOutputClicked);
    m_mainLayout->addWidget(m_viewButton);

    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setMinimumHeight(28);
    m_saveButton->setToolTip("Save printer output to file");
    connect(m_saveButton, &QPushButton::clicked, this, &PrinterWidget::onSaveOutputClicked);
    m_mainLayout->addWidget(m_saveButton);

    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setMinimumHeight(28);
    m_clearButton->setToolTip("Clear captured printer output");
    connect(m_clearButton, &QPushButton::clicked, this, &PrinterWidget::onClearOutputClicked);
    m_mainLayout->addWidget(m_clearButton);

    m_testButton = new QPushButton("Test", this);
    m_testButton->setMinimumHeight(28);
    m_testButton->setToolTip("Send test output to printer");
    connect(m_testButton, &QPushButton::clicked, this, &PrinterWidget::onPrintTestClicked);
    m_mainLayout->addWidget(m_testButton);

    setLayout(m_mainLayout);
}

void PrinterWidget::setPrinterEnabled(bool enabled)
{
    if (m_printerEnabled != enabled) {
        m_printerEnabled = enabled;
        m_enabledCheck->setChecked(enabled);
        updatePrinterStatus();
        updateControls();
        emit printerEnabledChanged(enabled);
        qDebug() << "Printer enabled changed to:" << enabled;
    }
}

void PrinterWidget::setOutputFormat(const QString& format)
{
    if (OUTPUT_FORMATS.contains(format) && m_outputFormat != format) {
        m_outputFormat = format;
        m_formatCombo->setCurrentText(format);
        updatePrinterStatus(); // Update status immediately
        emit outputFormatChanged(format);
        qDebug() << "Printer output format changed to:" << format;
    }
}

QString PrinterWidget::getOutputFormat() const
{
    return m_outputFormat;
}

void PrinterWidget::setPrinterType(const QString& type)
{
    if (PRINTER_TYPES.contains(type) && m_printerType != type) {
        m_printerType = type;
        m_typeCombo->setCurrentText(type);
        updatePrinterStatus(); // Update status immediately
        emit printerTypeChanged(type);
        qDebug() << "Printer type changed to:" << type;
    }
}

QString PrinterWidget::getPrinterType() const
{
    return m_printerType;
}

void PrinterWidget::appendText(const QString& text)
{
    if (text.isEmpty()) return;
    
    qDebug() << "PrinterWidget::appendText called with" << text.length() << "characters";

    // Check buffer size limit
    if (m_outputBuffer.size() + text.size() > MAX_BUFFER_SIZE) {
        // Truncate older content if needed
        int targetSize = MAX_BUFFER_SIZE / 2;
        m_outputBuffer = m_outputBuffer.right(targetSize);
        m_outputBuffer.prepend("... [Output truncated] ...\n\n");
    }

    m_outputBuffer.append(text);

    // Update output viewer if open
    if (m_outputViewer && m_outputViewer->isVisible() && m_outputDisplay) {
        m_outputDisplay->appendPlainText(text);

        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_outputDisplay->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    updateControls();
}

void PrinterWidget::clearOutput()
{
    qDebug() << "PrinterWidget::clearOutput() called";
    qDebug() << "Buffer size before clear:" << m_outputBuffer.size();
    
    m_outputBuffer.clear();
    
    qDebug() << "Buffer size after clear:" << m_outputBuffer.size();

    // Update output display if viewer is open
    if (m_outputDisplay) {
        qDebug() << "Clearing output display";
        m_outputDisplay->clear();
        // Force a refresh of the display content
        m_outputDisplay->setPlainText(formatTextForOutput(m_outputBuffer));
        qDebug() << "Output display cleared and refreshed";
    }

    updateControls();
    qDebug() << "Printer output cleared - controls updated";
}

QString PrinterWidget::getOutputText() const
{
    return m_outputBuffer;
}

void PrinterWidget::saveToFile()
{
    if (m_outputBuffer.isEmpty()) {
        QMessageBox::information(this, "Save Printer Output",
                                "No printer output to save.");
        return;
    }

    QString defaultName = QString("atari_printer_output_%1.txt")
                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Printer Output",
        QDir(m_lastSaveDir).filePath(defaultName),
        "Text Files (*.txt);;All Files (*.*)"
    );

    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);
        m_lastSaveDir = fileInfo.dir().absolutePath();

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << formatTextForOutput(m_outputBuffer);
            file.close();

            QMessageBox::information(this, "Save Printer Output",
                                    QString("Printer output saved to:\n%1").arg(fileName));
            qDebug() << "Printer output saved to:" << fileName;
        } else {
            QMessageBox::warning(this, "Save Error",
                                QString("Could not save file:\n%1\n\nError: %2")
                                .arg(fileName, file.errorString()));
        }
    }
}

void PrinterWidget::loadSettings()
{
    QSettings settings;

    // Load printer settings
    m_printerEnabled = settings.value("printer/enabled", false).toBool();
    m_outputFormat = settings.value("printer/outputFormat", DEFAULT_FORMAT).toString();
    m_printerType = settings.value("printer/type", DEFAULT_PRINTER_TYPE).toString();
    m_lastSaveDir = settings.value("printer/lastSaveDir",
                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    // Validate format
    if (!OUTPUT_FORMATS.contains(m_outputFormat)) {
        m_outputFormat = DEFAULT_FORMAT;
    }
    
    // Validate printer type
    if (!PRINTER_TYPES.contains(m_printerType)) {
        m_printerType = DEFAULT_PRINTER_TYPE;
    }

    // Update UI
    m_enabledCheck->setChecked(m_printerEnabled);
    m_formatCombo->setCurrentText(m_outputFormat);
    m_typeCombo->setCurrentText(m_printerType);

    qDebug() << "Printer settings loaded - Enabled:" << m_printerEnabled
             << "Format:" << m_outputFormat << "Type:" << m_printerType;
}

void PrinterWidget::saveSettings()
{
    QSettings settings;

    settings.setValue("printer/enabled", m_printerEnabled);
    settings.setValue("printer/outputFormat", m_outputFormat);
    settings.setValue("printer/type", m_printerType);
    settings.setValue("printer/lastSaveDir", m_lastSaveDir);

    qDebug() << "Printer settings saved - Enabled:" << m_printerEnabled
             << "Format:" << m_outputFormat << "Type:" << m_printerType;
}

void PrinterWidget::onPrinterToggled(bool enabled)
{
    setPrinterEnabled(enabled);
    saveSettings();
}

void PrinterWidget::onOutputFormatChanged(const QString& format)
{
    if (m_outputFormat != format) {
        m_outputFormat = format;
        updatePrinterStatus(); // Update status immediately
        emit outputFormatChanged(format);
        saveSettings();
        qDebug() << "Printer output format changed to:" << format;
    }
}

void PrinterWidget::onPrinterTypeChanged(const QString& type)
{
    if (m_printerType != type) {
        qDebug() << "Printer type changing from" << m_printerType << "to" << type;
        m_printerType = type;
        updatePrinterStatus(); // Update status immediately
        
        // If viewer is open, update its display immediately
        if (m_outputViewer && m_outputViewer->isVisible()) {
            qDebug() << "Viewer is open, updating display for new printer type";
            updateViewerForCurrentState();
        }
        
        emit printerTypeChanged(type);
        saveSettings();
        qDebug() << "Printer type changed to:" << type;
    }
}

void PrinterWidget::onViewOutputClicked()
{
    showOutputViewer();
}

void PrinterWidget::onSaveOutputClicked()
{
    saveToFile();
}

void PrinterWidget::onClearOutputClicked()
{
    int result = QMessageBox::question(this, "Clear Printer Output",
                                       "Are you sure you want to clear all printer output?",
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);

    if (result == QMessageBox::Yes) {
        clearOutput();
    }
}

void PrinterWidget::onPrintTestClicked()
{
    qDebug() << "PrinterWidget::onPrintTestClicked() called - Type:" << m_printerType << "Format:" << m_outputFormat;
    qDebug() << "Current buffer size before test:" << m_outputBuffer.size();
    
    // Determine column width based on printer type
    int columnWidth = 80; // Default for most printers
    if (m_printerType == "Atari 820") {
        columnWidth = 40; // 40-column thermal printer
    }
    
    QString testText;
    
    if (columnWidth == 40) {
        // 40-column test for Atari 820
        testText = QString("=== ATARI 820 TEST - %1 ===\n"
                          "Date: %2\n"
                          "Time: %3\n"
                          "Format: %4\n")
                   .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"))
                   .arg(QDate::currentDate().toString("yyyy-MM-dd"))
                   .arg(QTime::currentTime().toString("hh:mm:ss"))
                   .arg(m_outputFormat);

        // Add 40-column ruler
        testText += "1234567890123456789012345678901234567890\n";
        testText += "         1         2         3         4\n";
        testText += "\n";

        // Add 40-column test lines
        testText += "40-col test: ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
        testText += "Lower: abcdefghijklmnopqrstuvwxyz0123456\n";
        testText += "Symbols: !@#$%^&*()_+-=[]{}|\\:;\"'<>?,./\n";
        testText += "\n";

        // Add wrapped text for 40-column
        testText += "This is a sample document for the\n";
        testText += "Atari 820 thermal printer. Text wraps\n";
        testText += "at 40 characters to match the printer\n";
        testText += "specifications. Multiple lines test\n";
        testText += "various content and formatting.\n";
        testText += "\n";

        // Compact ASCII table for 40 columns
        testText += "ASCII Characters (selected):\n";
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 40; col++) {
                int ascii = 33 + (row * 40) + col;
                if (ascii <= 126) {
                    testText += QChar(ascii);
                }
            }
            testText += "\n";
        }
    } else {
        // 80-column test for other printers
        testText = QString("=== ATARI PRINTER TEST - %1 ===\n"
                          "Date: %2 | Time: %3 | Format: %4 | Type: %5\n")
                   .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"))
                   .arg(QDate::currentDate().toString("yyyy-MM-dd"))
                   .arg(QTime::currentTime().toString("hh:mm:ss"))
                   .arg(m_outputFormat)
                   .arg(m_printerType);

        // Add 80-column ruler
        testText += "123456789012345678901234567890123456789012345678901234567890123456789012345678901\n";
        testText += "         1         2         3         4         5         6         7         8\n";
        testText += "\n";

        // Add 80-column test lines
        testText += "Full 80-column line test - ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-[]\n";
        testText += "Lower case test line --- abcdefghijklmnopqrstuvwxyz9876543210}{|\\:;\"'<>?,./~`\n";
        testText += "Mixed content test line: The quick brown fox jumps over the lazy dog 1234567890\n";
        testText += "\n";

        // Add a text block
        testText += "This is a sample text document to test the Atari printer emulation. The text\n";
        testText += "should wrap properly at 80 columns and display correctly in both Text and Raw\n";
        testText += "output formats. Multiple lines help verify that the printer buffer handles\n";
        testText += "various content types including punctuation, numbers, and special characters.\n";
        testText += "\n";

        // Add ASCII table section
        testText += "ASCII Character Table (printable characters 32-126):\n";
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 32; col++) {
                int ascii = 32 + (row * 32) + col;
                if (ascii <= 126) {
                    testText += QChar(ascii);
                }
            }
            testText += "\n";
        }
    }

    testText += "\n=== END TEST ===\n\n";

    qDebug() << "Generated test text length:" << testText.length();
    qDebug() << "About to call appendText()";
    qDebug() << "First 100 chars of test text:" << testText.left(100);
    
    appendText(testText);
    
    qDebug() << "Buffer size after appendText:" << m_outputBuffer.size();
    qDebug() << "First 100 chars of buffer:" << m_outputBuffer.left(100);

    QString message = QString("%1-column test output sent to printer buffer.\nClick 'View Output' to see the result.")
                     .arg(columnWidth);
    QMessageBox::information(this, "Printer Test", message);
}

void PrinterWidget::updatePrinterStatus()
{
    if (m_printerEnabled) {
        m_statusLabel->setText(QString("Ready (%1)").arg(m_outputFormat));
        m_statusLabel->setStyleSheet("QLabel { color: #0a7d0a; font-size: 9px; font-weight: bold; }");
    } else {
        m_statusLabel->setText("Disabled");
        m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 9px; }");
    }
}

void PrinterWidget::updateControls()
{
    bool hasOutput = !m_outputBuffer.isEmpty();

    m_viewButton->setEnabled(m_printerEnabled && hasOutput);
    m_saveButton->setEnabled(m_printerEnabled && hasOutput);
    m_clearButton->setEnabled(m_printerEnabled && hasOutput);
    m_testButton->setEnabled(m_printerEnabled);

    m_formatCombo->setEnabled(m_printerEnabled);
    m_typeCombo->setEnabled(m_printerEnabled);

    updatePrinterStatus();
}

QString PrinterWidget::formatTextForOutput(const QString& rawText)
{
    qDebug() << "formatTextForOutput called - input size:" << rawText.size() << "format:" << m_outputFormat;
    
    QString formatted = rawText;

    if (m_outputFormat == "Text") {
        // Convert ATASCII control characters and ensure proper line endings
        formatted.replace('\r', '\n');
        formatted.replace("\n\n", "\n");
        qDebug() << "formatTextForOutput Text mode - output size:" << formatted.size();
    } else if (m_outputFormat == "Raw") {
        // Keep raw format but add header
        formatted = QString("=== RAW PRINTER OUTPUT ===\n"
                           "Generated: %1\n"
                           "Length: %2 bytes\n"
                           "==============================\n\n")
                    .arg(QDateTime::currentDateTime().toString())
                    .arg(rawText.size()) + rawText;
        qDebug() << "formatTextForOutput Raw mode - output size:" << formatted.size();
    }
    // PDF format would be handled here in the future

    qDebug() << "formatTextForOutput returning:" << formatted.left(50) << "...";
    return formatted;
}

void PrinterWidget::showOutputViewer()
{
    qDebug() << "PrinterWidget::showOutputViewer() called";
    qDebug() << "Current buffer size:" << m_outputBuffer.size();
    qDebug() << "Current printer type:" << m_printerType;
    
    if (!m_outputViewer) {
        // Create output viewer window
        m_outputViewer = new QWidget(nullptr, Qt::Window);
        m_outputViewer->setWindowTitle("Printer Output");
        m_outputViewer->setWindowIcon(QApplication::windowIcon());
        m_outputViewer->resize(800, 600);

        QVBoxLayout* layout = new QVBoxLayout(m_outputViewer);

        // Info label
        m_outputViewerInfo = new QLabel(m_outputViewer);
        m_outputViewerInfo->setStyleSheet("QLabel { font-weight: bold; margin-bottom: 5px; }");
        layout->addWidget(m_outputViewerInfo);

        // Text display
        m_outputDisplay = new QPlainTextEdit(m_outputViewer);
        m_outputDisplay->setReadOnly(true);
        m_outputDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(m_outputDisplay);

        // Control buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();

        QPushButton* saveBtn = new QPushButton("Save...", m_outputViewer);
        QPushButton* clearBtn = new QPushButton("Clear", m_outputViewer);
        QPushButton* closeBtn = new QPushButton("Close", m_outputViewer);

        connect(saveBtn, &QPushButton::clicked, this, &PrinterWidget::saveToFile);
        connect(clearBtn, &QPushButton::clicked, this, &PrinterWidget::onClearOutputClicked);
        connect(closeBtn, &QPushButton::clicked, m_outputViewer, &QWidget::close);

        buttonLayout->addWidget(saveBtn);
        buttonLayout->addWidget(clearBtn);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);

        layout->addLayout(buttonLayout);
        m_outputViewer->setLayout(layout);
    }

    // Show window FIRST
    qDebug() << "showOutputViewer: showing window";
    m_outputViewer->show();
    m_outputViewer->raise();
    m_outputViewer->activateWindow();
    
    // Process events to ensure window is fully shown
    QApplication::processEvents();
    qDebug() << "showOutputViewer: window visible after processEvents:" << m_outputViewer->isVisible();
    
    // NOW update the content
    qDebug() << "showOutputViewer: calling updateViewerForCurrentState";
    updateViewerForCurrentState();

    // Scroll to bottom
    QScrollBar* scrollBar = m_outputDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void PrinterWidget::updateViewerForCurrentState()
{
    if (!m_outputViewer) {
        qDebug() << "updateViewerForCurrentState: m_outputViewer is null";
        return;
    }
    if (!m_outputDisplay) {
        qDebug() << "updateViewerForCurrentState: m_outputDisplay is null";
        return;
    }
    if (!m_outputViewerInfo) {
        qDebug() << "updateViewerForCurrentState: m_outputViewerInfo is null";
        return;
    }
    
    qDebug() << "updateViewerForCurrentState: updating for type" << m_printerType << "format" << m_outputFormat;
    qDebug() << "updateViewerForCurrentState: viewer visible:" << m_outputViewer->isVisible();
    
    // Update info label with current format and type
    QString columnInfo = (m_printerType == "Atari 820") ? "40-column" : "80-column";
    QString infoText = QString("Printer Output (%1 format, %2 type, %3)")
                       .arg(m_outputFormat)
                       .arg(m_printerType)
                       .arg(columnInfo);
    qDebug() << "updateViewerForCurrentState: setting info label to:" << infoText;
    m_outputViewerInfo->setText(infoText);

    // Set font based on printer type
    QFont newFont;
    if (m_printerType == "Atari 820") {
        // Larger font for 40-column display
        newFont = QFont("Courier New", 11);
        qDebug() << "updateViewerForCurrentState: setting 40-column font (11pt)";
    } else {
        // Larger font for 80-column displays
        newFont = QFont("Courier New", 12);
        qDebug() << "updateViewerForCurrentState: setting 80-column font (12pt)";
    }
    m_outputDisplay->setFont(newFont);

    // Update display with current content
    qDebug() << "updateViewerForCurrentState: setting text, buffer size:" << m_outputBuffer.size();
    qDebug() << "updateViewerForCurrentState: buffer empty?" << m_outputBuffer.isEmpty();
    
    QString formattedText = formatTextForOutput(m_outputBuffer);
    qDebug() << "updateViewerForCurrentState: formatted text size:" << formattedText.size();
    qDebug() << "updateViewerForCurrentState: formatted text empty?" << formattedText.isEmpty();
    
    m_outputDisplay->setPlainText(formattedText);
    qDebug() << "updateViewerForCurrentState: text set successfully";
    
    // Force a repaint
    m_outputDisplay->repaint();
    m_outputViewer->repaint();
    qDebug() << "updateViewerForCurrentState: forced repaint";
}
