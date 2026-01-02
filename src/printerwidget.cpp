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
#include <QDesktopServices>
#include <QTimer>
#include <QFile>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QEasingCurve>
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>

const QStringList PrinterWidget::OUTPUT_FORMATS = {"Text", "Raw"};
const QString PrinterWidget::DEFAULT_FORMAT = "Text";

// All FujiNet-PC supported printer types
const QStringList PrinterWidget::PRINTER_TYPES = {
    "file printer (TRIM)",
    "file printer (ASCII)",
    "---",
    "Atari 820",
    "Atari 822",
    "Atari 825",
    "Atari 1020",
    "Atari 1025",
    "Atari 1027",
    "Atari 1029",
    "Atari XMM801",
    "Atari XDM121",
    "Epson 80",
    "Epson PrintShop",
    "Okimate 10",
    "---",
    "GRANTIC",           // PNG output printer
    "HTML printer",
    "HTML ATASCII printer"
};

const QString PrinterWidget::DEFAULT_PRINTER_TYPE = "Atari 825";
const int PrinterWidget::MAX_BUFFER_SIZE = 1000000; // 1MB limit
const int PrinterWidget::FORM_TIP_HEIGHT;   // Defined in header
const int PrinterWidget::FORM_MAX_HEIGHT;   // Defined in header

PrinterWidget::PrinterWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_enabledCheck(nullptr)
    , m_statusLabel(nullptr)
    , m_formatLayout(nullptr)
    , m_formatLabel(nullptr)
    , m_formatCombo(nullptr)
    , m_formDisplay(nullptr)
    , m_controlsLayout(nullptr)
    , m_saveButton(nullptr)
    , m_tearButton(nullptr)
    , m_outputViewer(nullptr)
    , m_outputDisplay(nullptr)
    , m_outputViewerInfo(nullptr)
    , m_printerEnabled(false)
    , m_outputFormat(DEFAULT_FORMAT)
    , m_printerType(DEFAULT_PRINTER_TYPE)
    , m_lastSaveDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
    , m_duplicateCount(0)
    , m_scrollAnimation(nullptr)
    , m_tearAnimation(nullptr)
    , m_slideAnimation(nullptr)
    , m_formYOffset(0)
    , m_formYPos(FORM_MAX_HEIGHT - FORM_TIP_HEIGHT)
    , m_maxDisplayHeight(FORM_TIP_HEIGHT)
{
    // Initialize animations
    m_scrollAnimation = new QPropertyAnimation(this, "formYOffset", this);
    m_scrollAnimation->setDuration(400);  // 400ms smooth scroll
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_tearAnimation = new QPropertyAnimation(this, "formYOffset", this);
    m_tearAnimation->setDuration(300);  // Quick 300ms tear
    m_tearAnimation->setEasingCurve(QEasingCurve::InQuad);

    m_slideAnimation = new QPropertyAnimation(this, "formYPos", this);
    m_slideAnimation->setDuration(400);  // 400ms smooth slide
    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // Load form image
    QString formPath = "/Users/pgarcia/dev/atari/fujisan/images/form-full.png";
    if (!m_formPixmap.load(formPath)) {
        qWarning() << "Failed to load continuous form image:" << formPath;
        // Fallback: create simple lined pattern matching form dimensions
        m_formPixmap = QPixmap(200, 200);
        m_formPixmap.fill(Qt::white);
        QPainter painter(&m_formPixmap);
        painter.setPen(QPen(Qt::lightGray, 1, Qt::DashLine));
        for (int y = 0; y < 200; y += 20) {
            painter.drawLine(0, y, 200, y);
        }
    }

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

    // Add items and insert separators at "---" markers
    for (const QString& type : PRINTER_TYPES) {
        if (type == "---") {
            m_typeCombo->insertSeparator(m_typeCombo->count());
        } else {
            m_typeCombo->addItem(type);
        }
    }

    m_typeCombo->setCurrentText(DEFAULT_PRINTER_TYPE);
    m_typeCombo->setMaximumHeight(24);
    m_typeCombo->setMinimumWidth(100);
    m_typeCombo->setToolTip("Select printer type (FujiNet-PC emulation):\n"
                            "Hardware emulation:\n"
                            "• Atari 820: 40-column thermal\n"
                            "• Atari 825/1025: 80-column dot matrix\n"
                            "• Atari 1020: 4-color plotter\n"
                            "• Epson 80: Epson emulation\n\n"
                            "Document output:\n"
                            "• PDF/PNG/SVG/HTML: Various formats\n\n"
                            "Raw file output:\n"
                            "• RAW/TRIM/ASCII: Text formats");
    connect(m_typeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &PrinterWidget::onPrinterTypeChanged);
    m_mainLayout->addWidget(m_typeCombo, 0, Qt::AlignLeft);

    // Output format selection - HIDDEN for FujiNet (format determined by printer type)
    QLabel* formatLabel = new QLabel("Format:", this);
    formatLabel->setStyleSheet("QLabel { font-size: 10px; }");
    formatLabel->setVisible(false);  // Hidden - not applicable for FujiNet
    m_mainLayout->addWidget(formatLabel);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems(OUTPUT_FORMATS);
    m_formatCombo->setCurrentText(DEFAULT_FORMAT);
    m_formatCombo->setMaximumHeight(24);
    m_formatCombo->setMinimumWidth(100);
    m_formatCombo->setVisible(false);  // Hidden - not applicable for FujiNet
    connect(m_formatCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &PrinterWidget::onOutputFormatChanged);
    m_mainLayout->addWidget(m_formatCombo, 0, Qt::AlignLeft);

    // Control buttons in a horizontal layout (ABOVE the form)
    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->setContentsMargins(0, 10, 0, 10);
    buttonRow->addStretch();

    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setMinimumHeight(24);
    m_saveButton->setToolTip("Save printer output to file");
    connect(m_saveButton, &QPushButton::clicked, this, &PrinterWidget::onSaveOutputClicked);
    buttonRow->addWidget(m_saveButton);

    m_tearButton = new QPushButton("Tear", this);
    m_tearButton->setMinimumHeight(24);
    m_tearButton->setToolTip("Tear off printed output (clears buffer)");
    connect(m_tearButton, &QPushButton::clicked, this, &PrinterWidget::onClearOutputClicked);
    buttonRow->addWidget(m_tearButton);

    buttonRow->addStretch();

    m_mainLayout->addLayout(buttonRow);

    // Add spacing between buttons and printer surface
    m_mainLayout->addSpacing(15);

    // Printer surface line (always visible, represents printer output slot)
    QLabel* printerSurface = new QLabel(this);
    printerSurface->setFixedSize(200, 3);
    printerSurface->setStyleSheet("QLabel { background: #333; border: none; }");
    m_mainLayout->addWidget(printerSurface, 0, Qt::AlignCenter);

    // Create a container widget for the paper area with fixed size
    QWidget* paperArea = new QWidget(this);
    paperArea->setFixedSize(200, FORM_MAX_HEIGHT);
    paperArea->setStyleSheet("QWidget { background: transparent; }");
    m_mainLayout->addWidget(paperArea, 0, Qt::AlignCenter);

    // Continuous form display - initially HIDDEN, positioned in paper area
    m_formDisplay = new QLabel(paperArea);
    m_formDisplay->setFixedSize(200, FORM_MAX_HEIGHT);  // Full size, but invisible initially
    m_formDisplay->setScaledContents(false);
    m_formDisplay->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_formDisplay->setStyleSheet("QLabel { border: 1px solid #ccc; background: white; }");
    m_formDisplay->setCursor(Qt::PointingHandCursor);
    m_formDisplay->installEventFilter(this);
    m_formDisplay->setVisible(false);  // Start hidden
    // Position at bottom of paper area initially (will slide up when visible)
    m_formDisplay->move(0, FORM_MAX_HEIGHT - FORM_TIP_HEIGHT);

    // Initialize form display
    updateFormDisplay();

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

        // Clear deduplication state when toggling printer
        if (enabled) {
            m_lastReceivedHash.clear();
            m_duplicateCount = 0;
            qDebug() << "Cleared deduplication state (printer re-enabled)";
            // Note: Printer connection (SSE/polling) is handled by configurePrinter()
        }
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

    // Trigger scroll animation
    scrollFormUp(20);  // Smaller scroll for text append

    updateControls();
}

void PrinterWidget::clearOutput()
{
    qDebug() << "PrinterWidget::clearOutput() called - playing tear animation";

    // Create opacity effect if not already set
    if (!m_formDisplay->graphicsEffect()) {
        QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(m_formDisplay);
        m_formDisplay->setGraphicsEffect(opacityEffect);
    }

    // Animate: move form up and fade out
    QPropertyAnimation* moveUp = new QPropertyAnimation(this, "formYOffset");
    moveUp->setDuration(300);
    moveUp->setStartValue(m_formYOffset);
    moveUp->setEndValue(m_formYOffset - 80);  // Move up 80px
    moveUp->setEasingCurve(QEasingCurve::InQuad);

    QPropertyAnimation* fadeOut = new QPropertyAnimation(m_formDisplay->graphicsEffect(), "opacity");
    fadeOut->setDuration(300);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    QParallelAnimationGroup* tearGroup = new QParallelAnimationGroup(this);
    tearGroup->addAnimation(moveUp);
    tearGroup->addAnimation(fadeOut);

    connect(tearGroup, &QParallelAnimationGroup::finished, this, [this, tearGroup]() {
        qDebug() << "Tear animation finished - hiding form";

        // Hide the form completely
        m_formDisplay->setVisible(false);

        // Reset form state
        m_formYOffset = 0;
        m_formYPos = FORM_MAX_HEIGHT - FORM_TIP_HEIGHT;  // Reset to bottom position
        m_maxDisplayHeight = FORM_TIP_HEIGHT;
        m_formDisplay->setFixedSize(200, FORM_TIP_HEIGHT);
        m_formDisplay->move(0, m_formYPos);  // Move back to bottom

        // Restore full opacity for next time
        if (m_formDisplay->graphicsEffect()) {
            static_cast<QGraphicsOpacityEffect*>(m_formDisplay->graphicsEffect())->setOpacity(1.0);
        }

        // Clear display buffers but PRESERVE hash to prevent re-showing torn data
        m_outputBuffer.clear();
        m_lastPrinterData.clear();    // Clear this - prevents pause on duplicates
        m_lastContentType.clear();
        // DON'T clear m_lastReceivedHash - keeps old data from re-appearing
        m_duplicateCount = 0;         // Reset counter so duplicates don't pause

        qDebug() << "Tear complete - display cleared, hash preserved to prevent re-showing torn data";

        // Request clear of FujiNet-PC printer buffer
        emit requestClearPrinterBuffer();

        // Note: No need to resume polling - SSE connection is continuous

        // Update output display if viewer is open
        if (m_outputDisplay) {
            qDebug() << "Clearing output display";
            m_outputDisplay->clear();
            m_outputDisplay->setPlainText(formatTextForOutput(m_outputBuffer));
        }

        updateControls();
        updatePrinterStatus();

        // Cleanup animation group
        tearGroup->deleteLater();
    });

    tearGroup->start();
}

void PrinterWidget::resetDeduplicationHash()
{
    qDebug() << "Resetting deduplication hash - server buffer cleared";
    m_lastReceivedHash.clear();
    m_duplicateCount = 0;
}

QString PrinterWidget::getOutputText() const
{
    return m_outputBuffer;
}

void PrinterWidget::saveToFile()
{
    // Check if we have FujiNet binary data or text buffer
    bool hasFujiNetData = !m_lastPrinterData.isEmpty();
    bool hasTextData = !m_outputBuffer.isEmpty();

    if (!hasFujiNetData && !hasTextData) {
        QMessageBox::information(this, "Printer Output", "No output to save.");
        return;
    }

    // Determine extension and filter based on content type (if from FujiNet) or printer type
    QString extension = "txt";
    QString filter;
    QByteArray dataToSave;

    if (hasFujiNetData) {
        // Use actual content type from FujiNet response
        if (m_lastContentType.contains("pdf", Qt::CaseInsensitive)) {
            extension = "pdf";
            filter = "PDF Files (*.pdf);;All Files (*.*)";
        }
        else if (m_lastContentType.contains("png", Qt::CaseInsensitive)) {
            extension = "png";
            filter = "PNG Images (*.png);;All Files (*.*)";
        }
        else if (m_lastContentType.contains("svg", Qt::CaseInsensitive)) {
            extension = "svg";
            filter = "SVG Files (*.svg);;All Files (*.*)";
        }
        else if (m_lastContentType.contains("html", Qt::CaseInsensitive)) {
            extension = "html";
            filter = "HTML Files (*.html);;All Files (*.*)";
        }
        else {
            extension = "txt";
            filter = "Text Files (*.txt);;All Files (*.*)";
        }
        dataToSave = m_lastPrinterData;
    }
    else {
        // Fallback to text buffer (for local test prints)
        extension = "txt";
        filter = "Text Files (*.txt);;All Files (*.*)";
        dataToSave = m_outputBuffer.toUtf8();
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString defaultName = QString("atari_printer_output_%1.%2").arg(timestamp).arg(extension);

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Printer Output",
        QDir(m_lastSaveDir).filePath(defaultName),
        filter
    );

    if (fileName.isEmpty()) {
        return;
    }

    // Save the file
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(dataToSave);
        file.close();

        // Update last save directory
        QFileInfo fileInfo(fileName);
        m_lastSaveDir = fileInfo.absolutePath();
        saveSettings();

        QMessageBox::information(this, "Save Printer Output",
                                QString("Printer output saved to:\n%1").arg(fileName));
        qDebug() << "Printer output saved to:" << fileName;
    } else {
        QMessageBox::warning(this, "Save Error",
                           QString("Failed to save file:\n%1").arg(file.errorString()));
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

    // If printer is enabled on startup, clear deduplication state for fresh start
    // This ensures stale state from previous session doesn't block initial polling
    if (m_printerEnabled) {
        m_lastReceivedHash.clear();
        m_duplicateCount = 0;
        qDebug() << "Printer enabled on startup - cleared deduplication state";
    }
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

        // Clear deduplication state when printer type changes
        // Different printer type may produce different output format
        m_lastReceivedHash.clear();
        m_duplicateCount = 0;
        qDebug() << "Cleared deduplication state for new printer type";

        updatePrinterStatus(); // Update status immediately

        // If viewer is open, update its display immediately
        if (m_outputViewer && m_outputViewer->isVisible()) {
            qDebug() << "Viewer is open, updating display for new printer type";
            updateViewerForCurrentState();
        }

        // Printer connection will be handled by configurePrinter() in MainWindow
        // when printerTypeChanged signal is received
        emit printerTypeChanged(type);
        saveSettings();
        qDebug() << "Printer type changed to:" << type;
    }
}

void PrinterWidget::onViewOutputClicked()
{
    // For text output, show the text viewer
    if (!m_outputBuffer.isEmpty()) {
        showOutputViewer();
        return;
    }

    // For binary output (PDF/PNG/SVG/HTML), save temp file and open in default viewer
    if (!m_lastPrinterData.isEmpty() && !m_lastContentType.isEmpty()) {
        QString extension = "bin";
        if (m_lastContentType.contains("pdf", Qt::CaseInsensitive)) extension = "pdf";
        else if (m_lastContentType.contains("png", Qt::CaseInsensitive)) extension = "png";
        else if (m_lastContentType.contains("svg", Qt::CaseInsensitive)) extension = "svg";
        else if (m_lastContentType.contains("html", Qt::CaseInsensitive)) extension = "html";

        // Save to temp file
        QString tempPath = QDir::temp().filePath(QString("fujisan_printer_preview.%1").arg(extension));
        QFile file(tempPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_lastPrinterData);
            file.close();

            // Open in default viewer
            QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
            qDebug() << "Opened printer output in default viewer:" << tempPath;
        } else {
            qWarning() << "Failed to create temp file for viewing:" << file.errorString();
        }
    }
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
        // Show "Print Ready" if we have output waiting
        if (!m_lastPrinterData.isEmpty() || !m_outputBuffer.isEmpty()) {
            QString formatName = "ready";
            if (!m_lastContentType.isEmpty()) {
                if (m_lastContentType.contains("pdf")) formatName = "PDF ready";
                else if (m_lastContentType.contains("png")) formatName = "PNG ready";
                else if (m_lastContentType.contains("svg")) formatName = "SVG ready";
                else if (m_lastContentType.contains("html")) formatName = "HTML ready";
                else if (m_lastContentType.contains("text")) formatName = "Text ready";
            }
            m_statusLabel->setText(QString("\u2713 %1").arg(formatName));  // ✓ checkmark
            m_statusLabel->setStyleSheet("QLabel { color: #0a7d0a; font-size: 9px; font-weight: bold; }");
        } else {
            m_statusLabel->setText(QString("Ready (%1)").arg(m_outputFormat));
            m_statusLabel->setStyleSheet("QLabel { color: #0a7d0a; font-size: 9px; font-weight: bold; }");
        }
    } else {
        m_statusLabel->setText("Disabled");
        m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 9px; }");
    }
}

void PrinterWidget::updateControls()
{
    bool hasTextOutput = !m_outputBuffer.isEmpty();
    bool hasBinaryOutput = !m_lastPrinterData.isEmpty();
    bool hasAnyOutput = hasTextOutput || hasBinaryOutput;

    m_saveButton->setEnabled(m_printerEnabled && hasAnyOutput);     // For any data
    m_tearButton->setEnabled(m_printerEnabled && hasAnyOutput);     // For any data

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

// FujiNet printer output handling

void PrinterWidget::displayPrinterOutput(const QByteArray& data, const QString& contentType)
{
    // qDebug() << "Received printer output:" << data.size() << "bytes, type:" << contentType;

    // Ignore very small responses (likely noise from polling, not actual print jobs)
    if (data.size() < 10) {
        // qDebug() << "Ignoring small response (< 10 bytes)";
        return;
    }

    // Deduplication: Check if this is the same data we already processed
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    if (hash == m_lastReceivedHash) {
        m_duplicateCount++;
        // qDebug() << "Ignoring duplicate printer output (count:" << m_duplicateCount << ")";

        // ONLY pause for VALID duplicates
        // Check m_lastPrinterData to ensure we're pausing on valid data, not invalid
        if (m_duplicateCount >= MAX_DUPLICATES_BEFORE_PAUSE && !m_lastPrinterData.isEmpty()) {
            qDebug() << "Pausing printer polling due to repeated duplicates";
            emit requestPausePrinterPolling();
        }
        return;
    }

    // NEW DATA - Validate BEFORE storing hash or processing
    if (!validatePrinterOutput(data, contentType)) {
        // Invalid data - DON'T store hash, DON'T pause polling
        // Just silently ignore and continue polling for valid data
        // qDebug() << "Ignoring invalid printer output";
        return;
    }

    // VALID NEW DATA - process normally
    m_duplicateCount = 0;
    m_lastReceivedHash = hash;
    m_lastProcessedTime = QDateTime::currentMSecsSinceEpoch();

    // Auto-clear previous output before processing new data
    // This mimics real printer behavior: new print replaces old one
    if (!m_lastPrinterData.isEmpty() || !m_outputBuffer.isEmpty()) {
        qDebug() << "New print job detected - auto-clearing previous output";
        m_outputBuffer.clear();
        m_lastPrinterData.clear();
        m_lastContentType.clear();
    }

    // Store the data for manual save
    m_lastPrinterData = data;
    m_lastContentType = contentType;

    // Enable controls now that we have output
    updateControls();

    // Show status message indicating print ready
    QString formatName = "output";
    if (contentType.contains("pdf", Qt::CaseInsensitive)) formatName = "PDF";
    else if (contentType.contains("png", Qt::CaseInsensitive)) formatName = "PNG";
    else if (contentType.contains("svg", Qt::CaseInsensitive)) formatName = "SVG";
    else if (contentType.contains("html", Qt::CaseInsensitive)) formatName = "HTML";
    else if (contentType.contains("text", Qt::CaseInsensitive)) formatName = "text";

    // Trigger animation to indicate printing
    // If form is hidden, make it visible and slide up from bottom
    if (!m_formDisplay->isVisible()) {
        qDebug() << "First print - making form visible and sliding up from bottom";
        m_formDisplay->setVisible(true);

        // Start with form at bottom (offset = 0)
        m_formYOffset = 0;
        m_maxDisplayHeight = FORM_TIP_HEIGHT;
        m_formDisplay->setFixedSize(200, FORM_TIP_HEIGHT);

        // Position at bottom initially
        m_formYPos = FORM_MAX_HEIGHT - FORM_TIP_HEIGHT;
        m_formDisplay->move(0, m_formYPos);

        // Animate sliding up to top
        m_slideAnimation->setStartValue(m_formYPos);
        m_slideAnimation->setEndValue(0);  // Slide to top
        m_slideAnimation->start();
    }

    scrollFormUp(40);  // Scroll 40px up

    qDebug() << "Print ready:" << formatName << data.size() << "bytes - form scrolled";

    // For text output, auto-show the viewer window
    if (contentType.contains("text", Qt::CaseInsensitive)) {
        QString text = QString::fromUtf8(data);
        appendText(text);
        showOutputViewer();  // Auto-show viewer window for text
    }
}

void PrinterWidget::showPrintingActivity()
{
    // Update status label temporarily
    QString oldStyleSheet = m_statusLabel->styleSheet();
    m_statusLabel->setText("Receiving output...");
    m_statusLabel->setStyleSheet("QLabel { color: orange; font-size: 9px; font-weight: bold; }");

    // Restore after 1 second
    QTimer::singleShot(1000, this, [this, oldStyleSheet]() {
        updatePrinterStatus();  // Restore normal status
    });
}

void PrinterWidget::savePrinterOutput(const QByteArray& data, const QString& extension)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("fujinet_printer_%1.%2")
                      .arg(timestamp)
                      .arg(extension);

    QSettings settings("8bitrelics", "Fujisan");
    QString lastDir = settings.value("printer/lastSaveDir", QDir::homePath()).toString();

    m_lastSavedPath = QDir(lastDir).filePath(filename);

    qDebug() << "Saving printer output to:" << m_lastSavedPath;

    QFile file(m_lastSavedPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        qDebug() << "Printer output saved successfully";

        // Update last save directory
        settings.setValue("printer/lastSaveDir", QFileInfo(m_lastSavedPath).absolutePath());
    } else {
        qWarning() << "Failed to save printer output:" << file.errorString();
    }
}

bool PrinterWidget::validatePrinterOutput(const QByteArray& data, const QString& contentType)
{
    // Validate PDF format
    if (contentType.contains("pdf", Qt::CaseInsensitive)) {
        // PDF must start with %PDF-
        if (!data.startsWith("%PDF-")) {
            qWarning() << "Invalid PDF: missing %PDF- header";
            return false;
        }
        // PDF should have minimum size (at least a few hundred bytes for header + trailer)
        if (data.size() < 200) {
            qWarning() << "PDF too small:" << data.size() << "bytes (minimum 200 expected)";
            return false;
        }
        qDebug() << "PDF validation passed:" << data.size() << "bytes";
        return true;
    }

    // Validate PNG format
    if (contentType.contains("png", Qt::CaseInsensitive)) {
        // PNG must start with PNG signature: 89 50 4E 47 (0x89 'P' 'N' 'G')
        if (data.size() < 8 || !data.startsWith("\x89PNG")) {
            qWarning() << "Invalid PNG: missing PNG signature";
            return false;
        }
        qDebug() << "PNG validation passed:" << data.size() << "bytes";
        return true;
    }

    // Validate SVG format
    if (contentType.contains("svg", Qt::CaseInsensitive)) {
        // SVG is XML, should start with <?xml or <svg
        QString text = QString::fromUtf8(data.left(100));  // Check first 100 chars
        if (!text.contains("<?xml", Qt::CaseInsensitive) &&
            !text.contains("<svg", Qt::CaseInsensitive)) {
            qWarning() << "Invalid SVG: missing XML or SVG tag";
            return false;
        }
        qDebug() << "SVG validation passed:" << data.size() << "bytes";
        return true;
    }

    // Validate HTML format
    if (contentType.contains("html", Qt::CaseInsensitive)) {
        // HTML should contain <html or <!DOCTYPE
        QString text = QString::fromUtf8(data.left(200));  // Check first 200 chars
        if (!text.contains("<html", Qt::CaseInsensitive) &&
            !text.contains("<!DOCTYPE", Qt::CaseInsensitive)) {
            qWarning() << "Invalid HTML: missing DOCTYPE or HTML tag";
            return false;
        }
        qDebug() << "HTML validation passed:" << data.size() << "bytes";
        return true;
    }

    // Text and unknown types - accept anything with reasonable size
    if (data.isEmpty()) {
        qWarning() << "Empty data received";
        return false;
    }

    qDebug() << "Generic validation passed for" << contentType << ":" << data.size() << "bytes";
    return true;
}

// Animation and form display methods

void PrinterWidget::setFormYOffset(int offset)
{
    m_formYOffset = offset;
    updateFormDisplay();  // Redraw with new offset
}

void PrinterWidget::setFormYPos(int pos)
{
    m_formYPos = pos;
    // Update the actual Y position of the form display widget
    if (m_formDisplay && m_formDisplay->parentWidget()) {
        m_formDisplay->move(0, pos);
    }
}

void PrinterWidget::updateFormDisplay()
{
    if (m_formPixmap.isNull()) return;

    // Don't update display if form is hidden (no print data)
    if (!m_formDisplay->isVisible()) return;

    // Create display pixmap matching current display height
    QPixmap displayPixmap(200, m_maxDisplayHeight);
    displayPixmap.fill(Qt::white);

    QPainter painter(&displayPixmap);

    // Show only the TOP portion of the form (the "tip")
    // As m_formYOffset increases (printing), more of the form becomes visible
    int formHeight = m_formPixmap.height();

    // Calculate how much form content to show based on scroll offset
    int visibleContentHeight = qMin(m_formYOffset + (m_maxDisplayHeight - 5), formHeight);

    // Draw from the top of the form
    if (visibleContentHeight > 0) {
        QRect sourceRect(0, 0, 200, visibleContentHeight);
        QRect targetRect(0, 0, 200, visibleContentHeight);
        painter.drawPixmap(targetRect, m_formPixmap, sourceRect);
    }

    m_formDisplay->setPixmap(displayPixmap);
}

void PrinterWidget::scrollFormUp(int scrollAmount)
{
    // Stop any running animation
    if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
        m_scrollAnimation->stop();
    }

    // Expand the viewport to show newly printed content
    expandFormDisplay(scrollAmount);

    int currentOffset = m_formYOffset;
    int newOffset = currentOffset + scrollAmount;

    // Don't wrap - just limit to form height
    int formHeight = m_formPixmap.height();
    if (newOffset > formHeight) {
        newOffset = formHeight;
    }

    m_scrollAnimation->setStartValue(currentOffset);
    m_scrollAnimation->setEndValue(newOffset);
    m_scrollAnimation->start();
}

bool PrinterWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_formDisplay && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // Trigger View action
            if (m_printerEnabled && (!m_outputBuffer.isEmpty() || !m_lastPrinterData.isEmpty())) {
                onViewOutputClicked();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void PrinterWidget::expandFormDisplay(int additionalHeight)
{
    // Gradually expand the viewport to show more paper
    m_maxDisplayHeight = qMin(m_maxDisplayHeight + additionalHeight, FORM_MAX_HEIGHT);

    // Resize the display widget
    m_formDisplay->setFixedSize(200, m_maxDisplayHeight);

    // Update display to show new content
    updateFormDisplay();
}
