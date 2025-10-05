/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef CARTRIDGEWIDGET_H
#define CARTRIDGEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QString>

class AtariEmulator;

class CartridgeWidget : public QWidget
{
    Q_OBJECT

public:
    enum CartridgeState {
        Off,        // No cartridge inserted (cartridgeoff.png)
        On          // Cartridge inserted and active (cartridgeon.png)
    };

    explicit CartridgeWidget(AtariEmulator* emulator, QWidget *parent = nullptr);

    // State management
    void setState(CartridgeState state);
    CartridgeState getState() const { return m_currentState; }

    // Cartridge operations
    void loadCartridge(const QString& cartridgePath);
    void ejectCartridge();

    // Cartridge info
    QString getCartridgePath() const { return m_cartridgePath; }
    bool hasCartridge() const { return !m_cartridgePath.isEmpty(); }

    // Size management
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void updateFromEmulator();

signals:
    void cartridgeInserted(const QString& cartridgePath);
    void cartridgeEjected();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onInsertCartridge();
    void onEjectCartridge();

private:
    void setupUI();
    void loadImages();
    void updateDisplay();
    void createContextMenu();
    void updateTooltip();
    bool isValidCartridgeFile(const QString& fileName) const;

    // Properties
    AtariEmulator* m_emulator;
    CartridgeState m_currentState;
    QString m_cartridgePath;

    // UI Components
    QLabel* m_imageLabel;
    QMenu* m_contextMenu;
    QAction* m_insertAction;
    QAction* m_ejectAction;

    // Images for different states
    QPixmap m_offImage;
    QPixmap m_onImage;

    // Constants
    static const int CARTRIDGE_WIDTH = 70;
    static const int CARTRIDGE_HEIGHT = 25;
};

#endif // CARTRIDGEWIDGET_H
