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
#include <QSlider>
#include <QSettings>
#include <QDockWidget>
#include <QTimer>
#include <QClipboard>
#include <QDialog>
#include <QPixmap>
#include "atariemulator.h"
#include "emulatorwidget.h"
#include "toggleswitch.h"
#include "settingsdialog.h"
#include "debuggerwidget.h"
#include "diskdrivewidget.h"
#include "joystickswapwidget.h"
#include "volumeknob.h"
#include "cartridgewidget.h"
#include "mediaperipheralsdock.h"
#include "configurationprofilemanager.h"

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
    void coldBoot();
    void warmBoot();
    void toggleBasic(bool enabled);
    void toggleAltirraOS(bool enabled);
    void onMachineTypeChanged(int index);
    void onVideoSystemToggled(bool isPAL);
    void onSpeedToggled(bool isFullSpeed);
    void showSettings();
    void onSettingsChanged();
    void toggleFullscreen();
    void showAbout();
    void toggleDebugger();
    void pasteText();
    void sendNextCharacter();
    void toggleMediaDock();
    void onDiskInserted(int driveNumber, const QString& diskPath);
    void onDiskEjected(int driveNumber);
    void onDriveStateChanged(int driveNumber, bool enabled);
    void onCassetteInserted(const QString& cassettePath);
    void onCassetteEjected();
    void onCassetteStateChanged(bool enabled);
    void onCartridgeInserted(const QString& cartridgePath);
    void onCartridgeEjected();
    void onLoadProfile();

private:
    void createMenus();
    void createToolBar();
    void createJoystickToolbarSection();
    void createAudioToolbarSection();
    void createProfileToolbarSection();
    void createLogoSection();
    void createEmulatorWidget();
    void createDebugger();
    void createMediaPeripheralsDock();
    void restartEmulator();
    void updateToolbarFromSettings();
    void loadInitialSettings();
    void loadAndApplyMediaSettings();
    void saveDiskToSettings(int driveNumber, const QString& diskPath, bool readOnly = false);
    void clearDiskFromSettings(int driveNumber);
    void saveDriveStateToSettings(int driveNumber, bool enabled);
    void loadVideoSettings();
    void enterCustomFullscreen();
    void exitCustomFullscreen();
    void sendTextToEmulator(const QString& text);
    void refreshProfileList();
    void applyProfileToEmulator(const ConfigurationProfile& profile);

    AtariEmulator* m_emulator;
    EmulatorWidget* m_emulatorWidget;
    QToolBar* m_toolBar;

    // Debugger
    DebuggerWidget* m_debuggerWidget;
    QDockWidget* m_debuggerDock;

    // Media & Peripherals Dock
    DiskDriveWidget* m_diskDrive1;              // D1 stays on toolbar
    CartridgeWidget* m_cartridgeWidget;         // Cartridge moved to toolbar
    MediaPeripheralsDock* m_mediaPeripheralsDock; // D2-D8, cassette, printer (cartridge moved out)
    QDockWidget* m_mediaPeripheralsDockWidget;
    QPushButton* m_mediaToggleButton;

    // Console buttons
    QPushButton* m_startButton;
    QPushButton* m_selectButton;
    QPushButton* m_optionButton;
    QPushButton* m_breakButton;

    // Toolbar widgets
    ToggleSwitch* m_basicToggle;
    QComboBox* m_machineCombo;
    ToggleSwitch* m_videoToggle;
    ToggleSwitch* m_speedToggle;

    // Joystick toolbar section
    QCheckBox* m_joystickEnabledCheck;
    QCheckBox* m_kbdJoy0Check;
    QCheckBox* m_kbdJoy1Check;
    JoystickSwapWidget* m_joystickSwapWidget;

    // Audio toolbar section
    VolumeKnob* m_volumeKnob;
    
    // Profile toolbar section
    QComboBox* m_profileCombo;
    QPushButton* m_loadProfileButton;
    ConfigurationProfileManager* m_profileManager;

    // Menu actions
    QAction* m_loadRomAction;
    QAction* m_coldBootAction;
    QAction* m_warmBootAction;
    QAction* m_basicAction;
    QAction* m_altirraOSAction;
    QAction* m_settingsAction;
    QAction* m_exitAction;
    QAction* m_aboutAction;
    QAction* m_pasteAction;

    // Video system actions
    QAction* m_videoNTSCAction;
    QAction* m_videoPALAction;

    // Video settings
    bool m_keepAspectRatio;
    bool m_startInFullscreen;

    // View menu actions
    QAction* m_fullscreenAction;
    QAction* m_debuggerAction;

    // Fullscreen state
    bool m_isInCustomFullscreen;
    QWidget* m_fullscreenWidget;

    // Paste functionality
    QTimer* m_pasteTimer;
    QString m_pasteBuffer;
    int m_pasteIndex;
    int m_originalEmulationSpeed;
    bool m_pasteCharacterSent;
};

#endif // MAINWINDOW_H
