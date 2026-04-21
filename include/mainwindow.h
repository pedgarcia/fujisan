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
#include <QThread>
#include <QClipboard>
#include <QDialog>
#include <QPixmap>
#include <QEvent>
#include <QShowEvent>
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
#include "fastbasicbuildpanel.h"
#include "fujinetservice.h"

class FujiNetProcessManager;
class FujiNetBinaryManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void coldBoot();
    void warmBoot();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void loadRom();
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

    // FujiNet slots
    void onFujiNetProcessStateChanged(int state);
    void onNetSIOEnabledChanged(bool enabled);
    void onFujiNetConnected();
    void onFujiNetDisconnected();
    void onFujiNetDriveStatusUpdated(const QVector<FujiNetDrive>& drives);
    void resetFujiNet();   // FujiNet Reset button — force-kill + restart FujiNet-PC
    void swapFujiNetDisks(); // FujiNet Swap button — rotate disk images via HTTP GET /swap

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
    void onProfileChanged(const QString& profileName);

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

    // Public method for TCP server to set H drive path (triggers emulator restart)
    bool setHardDrivePathViaTCP(int driveNumber, const QString& path);

    // Public accessor for build panel / tools that need to load XEX or access emulator
    AtariEmulator* getEmulator() const { return m_emulator; }

    // Public methods for TCP server to stop/start FujiNet-PC (e.g. stop → load XEX → start)
    void stopFujiNetViaTCP();
    void startFujiNetViaTCP();

private:
    void createMenus();
    void createToolBar();
    void createJoystickToolbarSection();
    void createAudioToolbarSection();
    void createProfileToolbarSection();
    void createLogoSection();
    void createStatusBarWidgets();
    void createEmulatorWidget();
    void createDebugger();
    void createMediaPeripheralsDock();
    void restartEmulator();
    void updateToolbarFromSettings();
    /// Read input/joystick* from QSettings and apply via AtariEmulator::applyJoystickInputBundle (worker thread).
    void syncEmulatorJoystickFromQSettings();
    void updateBasicToggleState();
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
    /// Apply profile settings to QSettings and the emulator. When \a fullRestart is false,
    /// skips restartEmulator() — used at startup after initializeWithNetSIOConfig() already
    /// matched this profile, to avoid tearing down libatari800 twice (double FujiNet boot).
    /// NetSIO toggles still force a full restart when old/new differ.
    void applyProfileToEmulator(const ConfigurationProfile& profile, bool fullRestart = true);

    // Status bar update methods
    void updateSpeedStatus();
    void updateFujiNetStatus();

    /// Return keyboard focus to the emulator after toolbar / dock / debugger interaction (Win/Linux).
    void restoreEmulatorFocus();
    void installChromeFocusRestore();
    /// Shutdown + teardown + delete the emulator on the worker, then quit/wait the
    /// thread. Sets m_emulator to nullptr. No-op if the worker is not running.
    void stopEmulatorWorkerIfRunning();
    /// Non-blocking: signal the worker thread to start tearing itself down (sets
    /// m_shuttingDown, queues finalizeShutdown, calls netsio_shutdown to unblock
    /// select()/read() in the worker). Idempotent; safe to call before quitting.
    void kickEmulatorWorkerShutdown();
    /// Blocking: wait for the worker thread quit() initiated by kickEmulatorWorkerShutdown(),
    /// then move/delete the emulator. Falls back to terminate()/detach if the worker
    /// is wedged. Sets m_emulator to nullptr. No-op if no worker exists.
    void joinEmulatorWorkerShutdown();
    /// Stops managed FujiNet-PC and only then kills orphaned external processes.
    /// MUST be called from the GUI thread (uses QProcess which is thread-affined).
    void shutdownFujiNetOnQuit();
    /// True after the first quit teardown so closeEvent + ~MainWindow do not duplicate work.
    bool m_quitTeardownDone = false;
    bool widgetIsUnderChrome(const QWidget *w) const;
    bool shouldSuppressChromeFocusRestore(const QWidget *w, QEvent::Type type) const;

    // FujiNet helper methods
    void updateFujiNetConfigFile(const QString& configPath, int netsioPort);
    void startFujiNetWithSavedSettings();
    void switchDrivesToFujiNetMode();
    void switchDrivesToLocalMode();
    void updateStatusBarForDriveMode();
    QString getFujiNetSDPath() const;  // Get SD path with default computation

    AtariEmulator* m_emulator;
    QThread* m_emulatorThread;
    EmulatorWidget* m_emulatorWidget;
    QToolBar* m_toolBar;
    
    // TCP Server for remote control
    TCPServer* m_tcpServer;

    // FujiNet-PC process management
    FujiNetProcessManager* m_fujinetProcessManager;
    FujiNetBinaryManager* m_fujinetBinaryManager;
    FujiNetService* m_fujinetService;  // Service for FujiNet API communication
    bool m_fujinetIntentionalRestart;  // Track intentional vs unexpected disconnects

    // Restart loop protection
    int m_fujinetRestartCount;
    qint64 m_fujinetFirstRestartTime;
    static const int MAX_RESTARTS = 3;
    static const int RESTART_WINDOW_MS = 10000; // 10 seconds

    // Fastbasic build panel (above status bar when enabled)
    FastbasicBuildPanel* m_fastbasicBuildPanel;

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
    QPushButton* m_startButton = nullptr;
    QPushButton* m_selectButton = nullptr;
    QPushButton* m_optionButton = nullptr;
    QPushButton* m_breakButton = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QPushButton* m_insertButton = nullptr;
    QPushButton* m_clearButton = nullptr;

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

    // Status bar widgets
    QLabel* m_speedStatusLabel;
    QLabel* m_fujinetStatusLabel;

    // Menu actions
    QAction* m_loadRomAction;
    QAction* m_coldBootAction;
    QAction* m_warmBootAction;
    QAction* m_pauseAction;
    QAction* m_specialOptionAction;
    QAction* m_specialSelectAction;
    QAction* m_specialStartAction;
    QAction* m_specialBreakAction;
    QAction* m_specialInsertAction;
    QAction* m_specialPauseAction;
    QAction* m_specialClearAction;
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
    EmulatorWidget* m_fullscreenEmulatorWidget;

    // NetSIO state tracking (for BASIC toggle disabling)
    bool m_netSIOEnabled;

    // Paste functionality
    QTimer* m_pasteTimer;
    QString m_pasteBuffer;
    int m_pasteIndex;
    int m_originalEmulationSpeed;
};

#endif // MAINWINDOW_H
