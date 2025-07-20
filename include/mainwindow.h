/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>
#include "atariemulator.h"
#include "emulatorwidget.h"
#include "toggleswitch.h"
#include "settingsdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void loadRom();
    void loadDiskImage();
    void coldBoot();
    void warmBoot();
    void toggleBasic(bool enabled);
    void toggleAltirraOS(bool enabled);
    void onMachineTypeChanged(int index);
    void onVideoSystemToggled(bool isPAL);
    void showSettings();
    void onSettingsChanged();
    void toggleFullscreen();
    void showAbout();

private:
    void createMenus();
    void createToolBar();
    void createEmulatorWidget();
    void restartEmulator();
    void updateToolbarFromSettings();
    void loadInitialSettings();
    void loadAndApplyMediaSettings();
    void loadVideoSettings();
    void enterCustomFullscreen();
    void exitCustomFullscreen();
    
    AtariEmulator* m_emulator;
    EmulatorWidget* m_emulatorWidget;
    QToolBar* m_toolBar;
    
    // Toolbar widgets
    ToggleSwitch* m_basicToggle;
    QComboBox* m_machineCombo;
    ToggleSwitch* m_videoToggle;
    
    // Menu actions
    QAction* m_loadRomAction;
    QAction* m_loadDiskAction;
    QAction* m_coldBootAction;
    QAction* m_warmBootAction;
    QAction* m_basicAction;
    QAction* m_altirraOSAction;
    QAction* m_settingsAction;
    QAction* m_exitAction;
    QAction* m_aboutAction;
    
    // Video system actions
    QAction* m_videoNTSCAction;
    QAction* m_videoPALAction;
    
    // Video settings
    bool m_keepAspectRatio;
    bool m_startInFullscreen;
    
    // View menu action
    QAction* m_fullscreenAction;
    
    // Fullscreen state
    bool m_isInCustomFullscreen;
    QWidget* m_fullscreenWidget;
};

#endif // MAINWINDOW_H