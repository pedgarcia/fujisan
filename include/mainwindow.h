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
#include "atariemulator.h"
#include "emulatorwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void loadRom();
    void coldBoot();
    void warmBoot();
    void toggleBasic(bool enabled);
    void toggleAltirraOS(bool enabled);
    void toggleAutoRestart(bool enabled);
    void restartEmulator();
    void setMachine800();
    void setMachineXL();
    void setMachineXE();
    void setMachine5200();
    void setVideoNTSC();
    void setVideoPAL();
    void showAbout();

private:
    void createMenus();
    void createToolBar();
    void createEmulatorWidget();
    
    AtariEmulator* m_emulator;
    EmulatorWidget* m_emulatorWidget;
    QToolBar* m_toolBar;
    bool m_autoRestart = false;
    
    // Menu actions
    QAction* m_loadRomAction;
    QAction* m_coldBootAction;
    QAction* m_warmBootAction;
    QAction* m_basicAction;
    QAction* m_altirraOSAction;
    QAction* m_autoRestartAction;
    QAction* m_restartAction;
    QAction* m_exitAction;
    QAction* m_aboutAction;
    
    // Machine type actions
    QAction* m_machine800Action;
    QAction* m_machineXLAction;
    QAction* m_machineXEAction;
    QAction* m_machine5200Action;
    
    // Video system actions
    QAction* m_videoNTSCAction;
    QAction* m_videoPALAction;
};

#endif // MAINWINDOW_H