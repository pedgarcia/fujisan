/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef PROFILESELECTIONWIDGET_H
#define PROFILESELECTIONWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include "configurationprofilemanager.h"

class ProfileSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileSelectionWidget(ConfigurationProfileManager* manager, QWidget *parent = nullptr);
    
    // Get current profile information
    QString getCurrentProfileName() const;
    void setCurrentProfileName(const QString& name);
    
    // UI state management
    void refreshProfileList();
    void updateUI();

signals:
    void profileChangeRequested(const QString& profileName);
    void saveCurrentProfile(const QString& profileName);
    void loadProfile(const QString& profileName);

public slots:
    void onProfileListChanged();

private slots:
    void onProfileComboChanged();
    void onSaveClicked();
    void onSaveAsClicked();
    void onLoadClicked();
    void onDeleteClicked();
    void onRenameClicked();

private:
    void setupUI();
    void connectSignals();
    void updateButtonStates();
    bool confirmDeleteProfile(const QString& profileName);
    QString promptForProfileName(const QString& title, const QString& label, const QString& defaultText = "");
    void showErrorMessage(const QString& title, const QString& message);
    void showInfoMessage(const QString& title, const QString& message);
    
    ConfigurationProfileManager* m_profileManager;
    
    // UI components
    QGroupBox* m_groupBox;
    QComboBox* m_profileCombo;
    QPushButton* m_saveButton;
    QPushButton* m_saveAsButton;
    QPushButton* m_loadButton;
    QPushButton* m_deleteButton;
    QPushButton* m_renameButton;
    QLabel* m_descriptionLabel;
    QLabel* m_lastUsedLabel;
    
    bool m_updatingCombo; // Flag to prevent recursive updates
};

#endif // PROFILESELECTIONWIDGET_H