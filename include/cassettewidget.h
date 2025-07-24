/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef CASSETTEWIDGET_H
#define CASSETTEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QString>

class AtariEmulator;

class CassetteWidget : public QWidget
{
    Q_OBJECT

public:
    enum CassetteState {
        Off,        // Cassette recorder is off (cassetteoff.png)
        On          // Cassette recorder is on (cassetteon.png)
    };

    explicit CassetteWidget(AtariEmulator* emulator, QWidget *parent = nullptr);
    
    // State management
    void setState(CassetteState state);
    CassetteState getState() const { return m_currentState; }
    
    // Cassette operations
    void loadCassette(const QString& cassettePath);
    void ejectCassette();
    void setCassetteEnabled(bool enabled);
    bool isCassetteEnabled() const { return m_cassetteEnabled; }
    
    // Cassette info
    QString getCassettePath() const { return m_cassettePath; }
    bool hasCassette() const { return !m_cassettePath.isEmpty(); }
    
    // Size management
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void updateFromEmulator();

signals:
    void cassetteInserted(const QString& cassettePath);
    void cassetteEjected();
    void cassetteStateChanged(bool enabled);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    
private slots:
    void onToggleCassette();
    void onInsertCassette();
    void onEjectCassette();
    
private:
    void setupUI();
    void loadImages();
    void updateDisplay();
    void createContextMenu();
    void updateTooltip();
    
    // Properties
    AtariEmulator* m_emulator;
    CassetteState m_currentState;
    bool m_cassetteEnabled;
    QString m_cassettePath;
    
    // UI Components
    QLabel* m_imageLabel;
    QMenu* m_contextMenu;
    QAction* m_toggleAction;
    QAction* m_insertAction;
    QAction* m_ejectAction;
    
    // Images for different states
    QPixmap m_offImage;
    QPixmap m_onImage;
    
    // Constants
    static const int CASSETTE_WIDTH = 120;
    static const int CASSETTE_HEIGHT = 80;
};

#endif // CASSETTEWIDGET_H