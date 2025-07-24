/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "cartridgewidget.h"
#include "atariemulator.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDebug>
#include <QApplication>
#include <QFileInfo>
#include <QPainter>

CartridgeWidget::CartridgeWidget(AtariEmulator* emulator, QWidget *parent)
    : QWidget(parent)
    , m_emulator(emulator)
    , m_currentState(Off)
    , m_imageLabel(nullptr)
    , m_contextMenu(nullptr)
{
    setupUI();
    loadImages();
    createContextMenu();
    
    // Initial state
    setState(Off);
    updateFromEmulator();
}

void CartridgeWidget::setupUI()
{
    setFixedSize(CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT);
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

void CartridgeWidget::loadImages()
{
    // Try multiple paths to find the images
    QStringList imagePaths = {
        "/Users/pgarcia/Documents/_priv/dev/atari/atari800-src/fujisan/images/",
        QApplication::applicationDirPath() + "/../fujisan/images/",
        QApplication::applicationDirPath() + "/images/",
        ":/images/",
        "./images/"
    };
    
    for (const QString& path : imagePaths) {
        QString offPath = path + "cartridgeoff.png";
        if (QFileInfo::exists(offPath)) {
            bool success = true;
            success &= m_offImage.load(offPath);
            success &= m_onImage.load(path + "cartridgeon.png");
            
            if (success) {
                qDebug() << "Successfully loaded cartridge images from:" << path;
                qDebug() << "Cartridge images loaded - Off image size:" << m_offImage.size();
                return;
            } else {
                qWarning() << "Some cartridge images failed to load from:" << path;
            }
        }
    }
    
    qWarning() << "Failed to load cartridge images! Searched paths:" << imagePaths;
    
    // Create fallback placeholder images if loading fails
    if (m_offImage.isNull()) {
        m_offImage = QPixmap(CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT);
        m_offImage.fill(Qt::gray);
        qDebug() << "Created fallback cartridge off image";
    }
}

void CartridgeWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_insertAction = new QAction("Insert Cartridge...", this);
    connect(m_insertAction, &QAction::triggered, this, &CartridgeWidget::onInsertCartridge);
    m_contextMenu->addAction(m_insertAction);
    
    m_ejectAction = new QAction("Eject", this);
    connect(m_ejectAction, &QAction::triggered, this, &CartridgeWidget::onEjectCartridge);
    m_contextMenu->addAction(m_ejectAction);
}

void CartridgeWidget::setState(CartridgeState state)
{
    if (m_currentState != state) {
        qDebug() << "Cartridge changing state from" << m_currentState << "to" << state;
        m_currentState = state;
        updateDisplay();
        updateTooltip();
    }
}

void CartridgeWidget::loadCartridge(const QString& cartridgePath)
{
    if (m_emulator && m_emulator->loadFile(cartridgePath)) {
        m_cartridgePath = cartridgePath;
        setState(On);
        updateTooltip();
        emit cartridgeInserted(cartridgePath);
        qDebug() << "Loaded cartridge:" << cartridgePath;
    }
}

void CartridgeWidget::ejectCartridge()
{
    if (hasCartridge()) {
        // TODO: Add libatari800 cartridge eject functionality when available
        m_cartridgePath.clear();
        setState(Off);
        updateTooltip();
        emit cartridgeEjected();
        qDebug() << "Ejected cartridge";
    }
}

void CartridgeWidget::updateFromEmulator()
{
    // TODO: Get current cartridge state from emulator when available
    updateTooltip();
}

void CartridgeWidget::updateDisplay()
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
        qDebug() << "Cartridge displaying state" << m_currentState << "image size:" << currentImage.size();
    } else {
        // Create a placeholder if image is missing
        QPixmap placeholder(size());
        placeholder.fill(Qt::lightGray);
        QPainter painter(&placeholder);
        painter.setPen(Qt::black);
        painter.drawText(placeholder.rect(), Qt::AlignCenter, 
            QString("CARTRIDGE\n%1").arg(m_currentState == Off ? "EMPTY" : "LOADED"));
        m_imageLabel->setPixmap(placeholder);
        qDebug() << "Cartridge showing placeholder for state" << m_currentState;
    }
}

void CartridgeWidget::updateTooltip()
{
    QString tooltip = "Cartridge Slot: ";
    
    switch (m_currentState) {
        case Off:
            tooltip += "Empty";
            break;
        case On:
            if (hasCartridge()) {
                QFileInfo fileInfo(m_cartridgePath);
                tooltip += QString("Loaded: %1").arg(fileInfo.fileName());
            } else {
                tooltip += "Loaded";
            }
            break;
    }
    
    setToolTip(tooltip);
}

QSize CartridgeWidget::sizeHint() const
{
    return QSize(CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT);
}

QSize CartridgeWidget::minimumSizeHint() const
{
    return QSize(CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT);
}

void CartridgeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    m_ejectAction->setEnabled(hasCartridge());
    m_contextMenu->exec(event->globalPos());
}

void CartridgeWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void CartridgeWidget::onInsertCartridge()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Cartridge Image",
        QString(),
        "Cartridge Images (*.rom *.ROM *.car *.CAR *.bin *.BIN);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        loadCartridge(fileName);
    }
}

void CartridgeWidget::onEjectCartridge()
{
    if (hasCartridge()) {
        ejectCartridge();
    }
}