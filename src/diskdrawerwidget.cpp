/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "diskdrawerwidget.h"
#include "atariemulator.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QDebug>

DiskDrawerWidget::DiskDrawerWidget(AtariEmulator* emulator, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_emulator(emulator)
    , m_containerFrame(nullptr)
    , m_gridLayout(nullptr)
    , m_targetWidget(nullptr)
    , m_eventFilterInstalled(false)
{
    // Initialize drive widgets array
    for (int i = 0; i < 6; i++) {
        m_driveWidgets[i] = nullptr;
    }
    
    setupUI();
    connectSignals();
    
    // Initially hidden
    hide();
    
    // Make it stay on top but not modal
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void DiskDrawerWidget::setupUI()
{
    // Create container frame with border
    m_containerFrame = new QFrame(this);
    m_containerFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
    m_containerFrame->setLineWidth(1);
    m_containerFrame->setStyleSheet(
        "QFrame {"
        "    background-color: palette(window);"
        "    border: 1px solid palette(dark);"
        "    border-radius: 4px;"
        "}"
    );
    
    // Main layout for the widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_containerFrame);
    
    // Grid layout inside the frame for the 3x2 matrix
    m_gridLayout = new QGridLayout(m_containerFrame);
    m_gridLayout->setContentsMargins(DRAWER_MARGIN, DRAWER_MARGIN, DRAWER_MARGIN, DRAWER_MARGIN);
    m_gridLayout->setSpacing(DRIVE_SPACING);
    
    // Create disk drive widgets D3-D8 in 3x2 matrix
    // Row 0: D3(0) D4(1) D5(2)
    // Row 1: D6(3) D7(4) D8(5)
    for (int i = 0; i < 6; i++) {
        int driveNumber = i + 3; // D3-D8
        int row = i / DRIVES_PER_ROW;
        int col = i % DRIVES_PER_ROW;
        
        m_driveWidgets[i] = new DiskDriveWidget(driveNumber, m_emulator, this, true); // true for drawer drives
        m_gridLayout->addWidget(m_driveWidgets[i], row, col);
        
        // Connect signals
        connect(m_driveWidgets[i], &DiskDriveWidget::diskInserted,
                this, &DiskDrawerWidget::onDiskInserted);
        connect(m_driveWidgets[i], &DiskDriveWidget::diskEjected,
                this, &DiskDrawerWidget::onDiskEjected);
        connect(m_driveWidgets[i], &DiskDriveWidget::driveStateChanged,
                this, &DiskDrawerWidget::onDriveStateChanged);
    }
    
    // Set fixed size based on content
    adjustSize();
    setFixedSize(sizeHint());
}

void DiskDrawerWidget::connectSignals()
{
    // No additional signals to connect at this level
}

void DiskDrawerWidget::showDrawer()
{
    if (!isVisible()) {
        updateAllDrives();
        show();
        installGlobalEventFilter();
        emit drawerVisibilityChanged(true);
        qDebug() << "Disk drawer opened at position:" << pos() << "size:" << size();
    }
}

void DiskDrawerWidget::hideDrawer()
{
    if (isVisible()) {
        hide();
        removeGlobalEventFilter();
        emit drawerVisibilityChanged(false);
        qDebug() << "Disk drawer closed";
    }
}

void DiskDrawerWidget::positionRelativeTo(QWidget* targetWidget)
{
    if (!targetWidget) return;
    
    m_targetWidget = targetWidget;
    
    // Find the toolbar widget to position relative to it instead of just the container
    QWidget* toolbar = targetWidget;
    while (toolbar && !toolbar->inherits("QToolBar")) {
        toolbar = toolbar->parentWidget();
    }
    
    if (toolbar) {
        // Position relative to the toolbar
        QPoint toolbarPos = toolbar->mapToGlobal(QPoint(0, 0));
        QSize toolbarSize = toolbar->size();
        
        // Position below the toolbar, aligned to the left edge of the target
        QPoint targetPos = targetWidget->mapToGlobal(QPoint(0, 0));
        int x = targetPos.x();
        int y = toolbarPos.y() + toolbarSize.height() + 4; // Position below entire toolbar
        
        move(x, y);
        qDebug() << "Positioning drawer at" << x << "," << y << "below toolbar (size:" << toolbarSize << ")";
    } else {
        // Fallback to original positioning if toolbar not found
        QPoint targetPos = targetWidget->mapToGlobal(QPoint(0, 0));
        QSize targetSize = targetWidget->size();
        
        int x = targetPos.x();
        int y = targetPos.y() + targetSize.height() + 4;
        
        move(x, y);
        qDebug() << "Positioning drawer at" << x << "," << y << "relative to target (fallback)";
    }
}

DiskDriveWidget* DiskDrawerWidget::getDriveWidget(int driveNumber)
{
    if (driveNumber >= 3 && driveNumber <= 8) {
        int index = driveNumber - 3;
        return m_driveWidgets[index];
    }
    return nullptr;
}

void DiskDrawerWidget::updateAllDrives()
{
    for (int i = 0; i < 6; i++) {
        if (m_driveWidgets[i]) {
            m_driveWidgets[i]->updateFromEmulator();
        }
    }
}

void DiskDrawerWidget::installGlobalEventFilter()
{
    if (!m_eventFilterInstalled) {
        QApplication::instance()->installEventFilter(this);
        m_eventFilterInstalled = true;
    }
}

void DiskDrawerWidget::removeGlobalEventFilter()
{
    if (m_eventFilterInstalled) {
        QApplication::instance()->removeEventFilter(this);
        m_eventFilterInstalled = false;
    }
}

bool DiskDrawerWidget::eventFilter(QObject* object, QEvent* event)
{
    // Close drawer when clicking outside of it
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        
        // Check if click is outside this widget and not on the target widget
        QWidget* clickedWidget = QApplication::widgetAt(mouseEvent->globalPos());
        
        if (clickedWidget && !this->isAncestorOf(clickedWidget) && 
            clickedWidget != this && clickedWidget != m_targetWidget) {
            hideDrawer();
            return false; // Don't consume the event
        }
    }
    
    return QWidget::eventFilter(object, event);
}

void DiskDrawerWidget::paintEvent(QPaintEvent* event)
{
    // Paint a subtle drop shadow effect
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw shadow
    QRect shadowRect = rect().adjusted(2, 2, 2, 2);
    painter.fillRect(shadowRect, QColor(0, 0, 0, 50));
    
    QWidget::paintEvent(event);
}

void DiskDrawerWidget::onDiskInserted(int driveNumber, const QString& diskPath)
{
    qDebug() << "Disk inserted in drive" << driveNumber << ":" << diskPath;
    // The signal will be automatically forwarded by the DiskDriveWidget
}

void DiskDrawerWidget::onDiskEjected(int driveNumber)
{
    qDebug() << "Disk ejected from drive" << driveNumber;
    // The signal will be automatically forwarded by the DiskDriveWidget
}

void DiskDrawerWidget::onDriveStateChanged(int driveNumber, bool enabled)
{
    qDebug() << "Drive" << driveNumber << "state changed to" << (enabled ? "on" : "off");
    // The signal will be automatically forwarded by the DiskDriveWidget
}