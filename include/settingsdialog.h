/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>
#include "atariemulator.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AtariEmulator* emulator, QWidget *parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void accept() override;
    void reject() override;
    void restoreDefaults();

private:
    void createMachineConfigTab();
    void createHardwareExtensionsTab();
    void createAudioConfigTab();
    void createVideoDisplayTab();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void updateVideoSystemDependentControls();
    
    AtariEmulator* m_emulator;
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;
    QPushButton* m_defaultsButton;
    
    // Machine Configuration controls
    QWidget* m_machineTab;
    QComboBox* m_machineTypeCombo;
    QComboBox* m_videoSystemCombo;
    QCheckBox* m_basicEnabledCheck;
    QCheckBox* m_altirraOSCheck;
    
    // Hardware Extensions controls
    QWidget* m_hardwareTab;
    QCheckBox* m_stereoPokey;
    QCheckBox* m_sioAcceleration;
    QCheckBox* m_rDeviceEnabled;
    QCheckBox* m_hDeviceEnabled;
    QCheckBox* m_pDeviceEnabled;
    
    // Audio Configuration controls  
    QWidget* m_audioTab;
    QCheckBox* m_soundEnabled;
    QComboBox* m_audioFrequency;
    QComboBox* m_audioBits;
    QCheckBox* m_consoleSound;
    QCheckBox* m_serialSound;
    
    // Video and Display controls
    QWidget* m_videoTab;
    QComboBox* m_artifactingMode;
    QCheckBox* m_showFPS;
    QCheckBox* m_scalingFilter;
    
    // PAL-specific controls
    QGroupBox* m_palGroup;
    QComboBox* m_palBlending;
    QCheckBox* m_palScanlines;
    
    // NTSC-specific controls  
    QGroupBox* m_ntscGroup;
    QComboBox* m_ntscArtifacting;
    QCheckBox* m_ntscSharpness;
    
    // Store original settings for cancel functionality
    struct OriginalSettings {
        QString machineType;
        QString videoSystem;
        bool basicEnabled;
        bool altirraOSEnabled;
    } m_originalSettings;
};

#endif // SETTINGSDIALOG_H