/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "cassettewidget.h"
#include "atariemulator.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QFileInfo>
#include <QPainter>
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>

CassetteWidget::CassetteWidget(AtariEmulator* emulator, QWidget *parent)
    : QWidget(parent)
    , m_emulator(emulator)
    , m_currentState(Off)
    , m_cassetteEnabled(false)
    , m_imageLabel(nullptr)
    , m_contextMenu(nullptr)
{
    setupUI();
    loadImages();
    createContextMenu();
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Initial state
    setState(Off);
    updateFromEmulator();
    
    // Force initial display update
    updateDisplay();
}

void CassetteWidget::setupUI()
{
    setFixedSize(CASSETTE_WIDTH, CASSETTE_HEIGHT);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_imageLabel);
}

void CassetteWidget::loadImages()
{
    // Try multiple relative paths to find the images
    QStringList imagePaths = {
        "./images/",
        "../images/",
        QApplication::applicationDirPath() + "/images/",
        QApplication::applicationDirPath() + "/../images/",
        ":/images/"
    };
    
    for (const QString& path : imagePaths) {
        QString offPath = path + "cassetteoff.png";
        QString onPath = path + "cassetteon.png";
        
        if (QFileInfo::exists(offPath) && QFileInfo::exists(onPath)) {
            bool success = true;
            success &= m_offImage.load(offPath);
            success &= m_onImage.load(onPath);
            
            if (success) {
                return;
            }
        }
    }
    
    // Create fallback placeholder images if loading fails
    if (m_offImage.isNull()) {
        m_offImage = QPixmap(CASSETTE_WIDTH, CASSETTE_HEIGHT);
        m_offImage.fill(Qt::gray);
    }
    if (m_onImage.isNull()) {
        m_onImage = QPixmap(CASSETTE_WIDTH, CASSETTE_HEIGHT);
        m_onImage.fill(Qt::lightGray);
    }
}

void CassetteWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &CassetteWidget::onToggleCassette);
    m_contextMenu->addAction(m_toggleAction);
    
    m_contextMenu->addSeparator();
    
    m_insertAction = new QAction("Insert Cassette...", this);
    connect(m_insertAction, &QAction::triggered, this, &CassetteWidget::onInsertCassette);
    m_contextMenu->addAction(m_insertAction);
    
    m_ejectAction = new QAction("Eject", this);
    connect(m_ejectAction, &QAction::triggered, this, &CassetteWidget::onEjectCassette);
    m_contextMenu->addAction(m_ejectAction);
}

void CassetteWidget::setState(CassetteState state)
{
    if (m_currentState != state) {
        qDebug() << "Cassette changing state from" << m_currentState << "to" << state;
        m_currentState = state;
        updateDisplay();
        updateTooltip();
    }
}

void CassetteWidget::loadCassette(const QString& cassettePath)
{
    // TODO: Integrate with libatari800 cassette loading when available
    m_cassettePath = cassettePath;
    setState(m_cassetteEnabled ? On : Off);
    updateTooltip();
    emit cassetteInserted(cassettePath);
    qDebug() << "Loaded cassette:" << cassettePath;
}

void CassetteWidget::ejectCassette()
{
    if (hasCassette()) {
        // TODO: Add libatari800 cassette eject functionality when available
        m_cassettePath.clear();
        setState(Off);
        updateTooltip();
        emit cassetteEjected();
        qDebug() << "Ejected cassette";
    }
}

void CassetteWidget::setCassetteEnabled(bool enabled)
{
    if (m_cassetteEnabled != enabled) {
        m_cassetteEnabled = enabled;
        setState(enabled && hasCassette() ? On : Off);
        emit cassetteStateChanged(enabled);
    }
}

void CassetteWidget::updateFromEmulator()
{
    // TODO: Get current cassette state from emulator when available
    updateTooltip();
}

void CassetteWidget::updateDisplay()
{
    if (!m_imageLabel) return;
    
    QPixmap currentImage;
    switch (m_currentState) {
        case Off:
            currentImage = m_offImage;
            break;
        case On:
            currentImage = m_onImage;
            break;
    }
    
    if (!currentImage.isNull()) {
        QPixmap scaledImage = currentImage.scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaledImage);
        qDebug() << "Cassette displaying state" << m_currentState << "image size:" << currentImage.size();
    } else {
        // Create a placeholder if image is missing
        QPixmap placeholder(size());
        placeholder.fill(Qt::lightGray);
        QPainter painter(&placeholder);
        painter.setPen(Qt::black);
        painter.drawText(placeholder.rect(), Qt::AlignCenter, 
            QString("CASSETTE\n%1").arg(m_currentState == Off ? "OFF" : "ON"));
        m_imageLabel->setPixmap(placeholder);
        qDebug() << "Cassette showing placeholder for state" << m_currentState;
    }
}

void CassetteWidget::updateTooltip()
{
    QString tooltip = "Cassette Recorder: ";
    
    switch (m_currentState) {
        case Off:
            tooltip += "Off";
            break;
        case On:
            if (hasCassette()) {
                QFileInfo fileInfo(m_cassettePath);
                tooltip += QString("Playing %1").arg(fileInfo.fileName());
            } else {
                tooltip += "On (No Cassette)";
            }
            break;
    }
    
    setToolTip(tooltip);
}

QSize CassetteWidget::sizeHint() const
{
    return QSize(CASSETTE_WIDTH, CASSETTE_HEIGHT);
}

QSize CassetteWidget::minimumSizeHint() const
{
    return QSize(CASSETTE_WIDTH, CASSETTE_HEIGHT);
}

void CassetteWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // Update menu items based on current state
    if (m_cassetteEnabled) {
        m_toggleAction->setText("Turn Off");
    } else {
        m_toggleAction->setText("Turn On");
    }
    
    m_insertAction->setEnabled(m_cassetteEnabled);
    m_ejectAction->setEnabled(m_cassetteEnabled && hasCassette());
    
    m_contextMenu->exec(event->globalPos());
}

void CassetteWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void CassetteWidget::onToggleCassette()
{
    setCassetteEnabled(!m_cassetteEnabled);
}

void CassetteWidget::onInsertCassette()
{
    if (!m_cassetteEnabled) return;
    
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Cassette Image",
        QString(),
        "Cassette Images (*.cas *.CAS *.wav *.WAV);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        loadCassette(fileName);
    }
}

void CassetteWidget::onEjectCassette()
{
    if (m_cassetteEnabled && hasCassette()) {
        ejectCassette();
    }
}

void CassetteWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // Only accept file drops if cassette is enabled
    if (!m_cassetteEnabled) {
        event->ignore();
        return;
    }
    
    // Check if we have file URLs
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidCassetteFile(fileName)) {
                event->acceptProposedAction();
                setStyleSheet("QWidget { border: 2px dashed #0078d4; background-color: rgba(0, 120, 212, 0.1); }");
                return;
            }
        }
    }
    event->ignore();
}

void CassetteWidget::dragMoveEvent(QDragMoveEvent* event)
{
    // Only accept if cassette is enabled and file is valid
    if (!m_cassetteEnabled) {
        event->ignore();
        return;
    }
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidCassetteFile(fileName)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void CassetteWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    // Clear visual feedback when drag leaves the widget
    setStyleSheet("");
    QWidget::dragLeaveEvent(event);
}

void CassetteWidget::dropEvent(QDropEvent* event)
{
    // Clear visual feedback
    setStyleSheet("");
    
    // Only accept if cassette is enabled
    if (!m_cassetteEnabled) {
        event->ignore();
        return;
    }
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1) {
            QString fileName = urls.first().toLocalFile();
            if (isValidCassetteFile(fileName)) {
                loadCassette(fileName);
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

bool CassetteWidget::isValidCassetteFile(const QString& fileName) const
{
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix().toLower();
    
    // Valid Atari cassette extensions
    QStringList validExtensions = {"cas", "wav"};
    
    return validExtensions.contains(extension);
}