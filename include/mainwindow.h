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
#include "tcpserver.h"

#ifndef Q_OS_WIN
class FujiNetProcessManager;
class FujiNetBinaryManager;
#endif

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
    void changeEvent(QEvent *event) override;

private slots:
    void loadRom();
    void coldBoot();
    void warmBoot();
    void toggleBasic(bool enabled);
    void toggleAltirraOS(bool enabled);
    void toggleAltirraBASIC(bool enabled);
    void onMachineTypeChanged(int index);
    void onVideoSystemToggled(bool isPAL);
    void onSpeedToggled(bool isFullSpeed);
    void showSettings();
    void onSettingsChanged();
    void toggleFullscreen();
    void showAbout();
    void toggleDebugger();
    void toggleTCPServer();
    void pasteText();
    void sendNextCharacter();
    void toggleMediaDock();
    void togglePause();

#ifndef Q_OS_WIN
    // FujiNet slots
    void onFujiNetProcessStateChanged(int state);
    void onNetSIOEnabledChanged(bool enabled);
#endif

    // State save/load slots
    void quickSaveState();
    void quickLoadState();
    void saveState();
    void loadState();
    void onDiskInserted(int driveNumber, const QString& diskPath);
    void onDiskEjected(int driveNumber);
    void onDiskDroppedOnEmulator(const QString& filename);
    void onDriveStateChanged(int driveNumber, bool enabled);
    void onCassetteInserted(const QString& cassettePath);
    void onCassetteEjected();
    void onCassetteStateChanged(bool enabled);
    void onCartridgeInserted(const QString& cartridgePath);
    void onCartridgeEjected();
    void onPrinterEnabledChanged(bool enabled);
    void onPrinterOutputFormatChanged(const QString& format);
    void onPrinterTypeChanged(const QString& type);
    void onLoadProfile();

public:
    // Public method for TCP server to request proper emulator restart
    void requestEmulatorRestart();
    
    // Public methods for TCP server to control disk drives properly
    bool insertDiskViaTCP(int driveNumber, const QString& diskPath);
    bool ejectDiskViaTCP(int driveNumber);
    bool enableDriveViaTCP(int driveNumber, bool enabled);
    
    // Public methods for TCP server to control cartridge properly
    bool insertCartridgeViaTCP(const QString& cartridgePath);
    bool ejectCartridgeViaTCP();
    
    // Public method for TCP server to access profile manager
    ConfigurationProfileManager* getProfileManager() const { return m_profileManager; }
    
    // Public method for TCP server to apply a profile
    void applyProfileViaTCP(const ConfigurationProfile& profile);

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
    void updateToolbarLogo();
    void updateToolbarButtonStyles();
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
    
    // TCP Server for remote control
    TCPServer* m_tcpServer;

#ifndef Q_OS_WIN
    // FujiNet-PC process management
    FujiNetProcessManager* m_fujinetProcessManager;
    FujiNetBinaryManager* m_fujinetBinaryManager;

    // Restart loop protection
    int m_fujinetRestartCount;
    qint64 m_fujinetFirstRestartTime;
    static const int MAX_RESTARTS = 3;
    static const int RESTART_WINDOW_MS = 10000; // 10 seconds
#endif

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
    QPushButton* m_pauseButton;

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
    QPushButton* m_quickSaveButton;
    QPushButton* m_quickLoadButton;
    ConfigurationProfileManager* m_profileManager;
    
    // Logo toolbar section
    QLabel* m_logoLabel;

    // Menu actions
    QAction* m_loadRomAction;
    QAction* m_coldBootAction;
    QAction* m_warmBootAction;
    QAction* m_pauseAction;
    QAction* m_basicAction;
    QAction* m_altirraOSAction;
    QAction* m_altirraBASICAction;
    QAction* m_settingsAction;
    QAction* m_exitAction;
    QAction* m_aboutAction;
    QAction* m_pasteAction;
    
    // State save/load actions
    QAction* m_quickSaveStateAction;
    QAction* m_quickLoadStateAction;
    QAction* m_saveStateAction;
    QAction* m_loadStateAction;

    // Video settings
    bool m_keepAspectRatio;
    bool m_startInFullscreen;

    // View menu actions
    QAction* m_fullscreenAction;
    QAction* m_debuggerAction;
    
    // TCP Server actions
    QAction* m_tcpServerAction;

    // Fullscreen state
    bool m_isInCustomFullscreen;
    QWidget* m_fullscreenWidget;

    // Paste functionality
    QTimer* m_pasteTimer;
    QString m_pasteBuffer;
    int m_pasteIndex;
    int m_originalEmulationSpeed;
};

#endif // MAINWINDOW_H
