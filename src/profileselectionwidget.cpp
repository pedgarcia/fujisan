/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "profileselectionwidget.h"
#include <QDebug>

ProfileSelectionWidget::ProfileSelectionWidget(ConfigurationProfileManager* manager, QWidget *parent)
    : QWidget(parent)
    , m_profileManager(manager)
    , m_groupBox(nullptr)
    , m_profileCombo(nullptr)
    , m_saveButton(nullptr)
    , m_saveAsButton(nullptr)
    , m_loadButton(nullptr)
    , m_deleteButton(nullptr)
    , m_renameButton(nullptr)
    , m_descriptionLabel(nullptr)
    , m_lastUsedLabel(nullptr)
    , m_updatingCombo(false)
{
    setupUI();
    connectSignals();
    refreshProfileList();
    updateUI();
}

QString ProfileSelectionWidget::getCurrentProfileName() const
{
    return m_profileCombo->currentText();
}

void ProfileSelectionWidget::setCurrentProfileName(const QString& name)
{
    if (m_updatingCombo) {
        return;
    }
    
    m_updatingCombo = true;
    
    int index = m_profileCombo->findText(name);
    if (index >= 0) {
        m_profileCombo->setCurrentIndex(index);
    } else {
        // Profile doesn't exist, refresh list and try again
        refreshProfileList();
        index = m_profileCombo->findText(name);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
        } else {
            // Still not found, select Default
            index = m_profileCombo->findText("Default");
            if (index >= 0) {
                m_profileCombo->setCurrentIndex(index);
            }
        }
    }
    
    m_updatingCombo = false;
    updateUI();
}

void ProfileSelectionWidget::refreshProfileList()
{
    if (m_updatingCombo) {
        return;
    }
    
    m_updatingCombo = true;
    
    QString currentProfile = m_profileCombo->currentText();
    
    m_profileCombo->clear();
    QStringList profiles = m_profileManager->getProfileNames();
    
    for (const QString& profile : profiles) {
        m_profileCombo->addItem(profile);
    }
    
    // Restore selection if possible
    if (!currentProfile.isEmpty()) {
        int index = m_profileCombo->findText(currentProfile);
        if (index >= 0) {
            m_profileCombo->setCurrentIndex(index);
        }
    }
    
    m_updatingCombo = false;
    updateUI();
}

void ProfileSelectionWidget::updateUI()
{
    QString currentProfile = getCurrentProfileName();
    
    // Update description and last used info
    if (!currentProfile.isEmpty() && m_profileManager->profileExists(currentProfile)) {
        QString description = m_profileManager->getProfileDescription(currentProfile);
        QDateTime lastUsed = m_profileManager->getProfileLastUsed(currentProfile);
        
        if (description.isEmpty()) {
            m_descriptionLabel->setText("No description");
        } else {
            m_descriptionLabel->setText(description);
        }
        
        if (lastUsed.isValid()) {
            m_lastUsedLabel->setText(QString("Last used: %1").arg(lastUsed.toString("MMM dd, yyyy hh:mm")));
        } else {
            m_lastUsedLabel->setText("Last used: Never");
        }
    } else {
        m_descriptionLabel->setText("");
        m_lastUsedLabel->setText("");
    }
    
    updateButtonStates();
}

void ProfileSelectionWidget::onProfileListChanged()
{
    refreshProfileList();
}

void ProfileSelectionWidget::onProfileComboChanged()
{
    if (m_updatingCombo) {
        return;
    }
    
    QString profileName = getCurrentProfileName();
    if (!profileName.isEmpty()) {
        emit profileChangeRequested(profileName);
        qDebug() << "Profile change requested:" << profileName;
    }
    
    updateUI();
}

void ProfileSelectionWidget::onSaveClicked()
{
    QString profileName = getCurrentProfileName();
    
    if (profileName.isEmpty()) {
        showErrorMessage("No Profile Selected", "Please select a profile to save to.");
        return;
    }
    
    if (profileName == "Default") {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Save to Default Profile",
            "Are you sure you want to overwrite the Default profile with current settings?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    emit saveCurrentProfile(profileName);
    refreshProfileList();
    setCurrentProfileName(profileName);
    
    showInfoMessage("Profile Saved", QString("Profile '%1' has been updated successfully.").arg(profileName));
    qDebug() << "Profile save requested (overwrite):" << profileName;
}

void ProfileSelectionWidget::onSaveAsClicked()
{
    QString defaultName = m_profileManager->generateUniqueProfileName("New Profile");
    QString profileName = promptForProfileName("Save Profile As", "Profile name:", defaultName);
    
    if (profileName.isEmpty()) {
        return; // User cancelled
    }
    
    // Sanitize the name
    profileName = m_profileManager->sanitizeProfileName(profileName);
    
    if (!m_profileManager->isValidProfileName(profileName)) {
        showErrorMessage("Invalid Name", "The profile name contains invalid characters or is too long.");
        return;
    }
    
    // Check if profile already exists
    if (m_profileManager->profileExists(profileName)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, 
            "Profile Exists", 
            QString("A profile named '%1' already exists. Do you want to overwrite it?").arg(profileName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    emit saveCurrentProfile(profileName);
    refreshProfileList();
    setCurrentProfileName(profileName);
    
    showInfoMessage("Profile Saved", QString("Profile '%1' has been saved successfully.").arg(profileName));
    qDebug() << "Profile save requested:" << profileName;
}

void ProfileSelectionWidget::onLoadClicked()
{
    QString profileName = getCurrentProfileName();
    
    if (profileName.isEmpty()) {
        showErrorMessage("No Profile Selected", "Please select a profile to load.");
        return;
    }
    
    if (!m_profileManager->profileExists(profileName)) {
        showErrorMessage("Profile Not Found", QString("The profile '%1' does not exist.").arg(profileName));
        refreshProfileList();
        return;
    }
    
    emit loadProfile(profileName);
    showInfoMessage("Profile Loaded", QString("Profile '%1' has been loaded successfully.").arg(profileName));
    qDebug() << "Profile load requested:" << profileName;
}

void ProfileSelectionWidget::onDeleteClicked()
{
    QString profileName = getCurrentProfileName();
    
    if (profileName.isEmpty()) {
        showErrorMessage("No Profile Selected", "Please select a profile to delete.");
        return;
    }
    
    if (profileName == "Default") {
        showErrorMessage("Cannot Delete", "The Default profile cannot be deleted.");
        return;
    }
    
    if (!confirmDeleteProfile(profileName)) {
        return; // User cancelled
    }
    
    bool success = m_profileManager->deleteProfile(profileName);
    
    if (success) {
        refreshProfileList();
        // Select Default profile after deletion
        setCurrentProfileName("Default");
        showInfoMessage("Profile Deleted", QString("Profile '%1' has been deleted successfully.").arg(profileName));
        qDebug() << "Profile deleted:" << profileName;
    } else {
        showErrorMessage("Delete Failed", QString("Failed to delete profile '%1'.").arg(profileName));
    }
}

void ProfileSelectionWidget::onRenameClicked()
{
    QString oldName = getCurrentProfileName();
    
    if (oldName.isEmpty()) {
        showErrorMessage("No Profile Selected", "Please select a profile to rename.");
        return;
    }
    
    if (oldName == "Default") {
        showErrorMessage("Cannot Rename", "The Default profile cannot be renamed.");
        return;
    }
    
    QString newName = promptForProfileName("Rename Profile", "New profile name:", oldName);
    
    if (newName.isEmpty() || newName == oldName) {
        return; // User cancelled or no change
    }
    
    // Sanitize the name
    newName = m_profileManager->sanitizeProfileName(newName);
    
    if (!m_profileManager->isValidProfileName(newName)) {
        showErrorMessage("Invalid Name", "The profile name contains invalid characters or is too long.");
        return;
    }
    
    if (m_profileManager->profileExists(newName)) {
        showErrorMessage("Name Exists", QString("A profile named '%1' already exists.").arg(newName));
        return;
    }
    
    bool success = m_profileManager->renameProfile(oldName, newName);
    
    if (success) {
        refreshProfileList();
        setCurrentProfileName(newName);
        showInfoMessage("Profile Renamed", QString("Profile renamed from '%1' to '%2' successfully.").arg(oldName, newName));
        qDebug() << "Profile renamed:" << oldName << "to" << newName;
    } else {
        showErrorMessage("Rename Failed", QString("Failed to rename profile '%1'.").arg(oldName));
    }
}

void ProfileSelectionWidget::setupUI()
{
    // Create group box
#ifdef Q_OS_MACOS
    m_groupBox = new QGroupBox("Preference Profiles");
#else
    m_groupBox = new QGroupBox("Settings Profiles");
#endif
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_groupBox);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    QVBoxLayout* groupLayout = new QVBoxLayout(m_groupBox);
    groupLayout->setSpacing(4); // Tight spacing between rows
    
    // Profile selection row - dropdown takes remaining space
    QHBoxLayout* selectionLayout = new QHBoxLayout();
    
    QLabel* profileLabel = new QLabel("Profile:");
    m_profileCombo = new QComboBox();
    m_profileCombo->setToolTip("Select a configuration profile");
    
    selectionLayout->addWidget(profileLabel);
    selectionLayout->addWidget(m_profileCombo, 1); // Expand to fill space
    
    groupLayout->addLayout(selectionLayout);
    
    // Buttons row with description on the right
    QHBoxLayout* buttonDescLayout = new QHBoxLayout();
    buttonDescLayout->setSpacing(12); // Spacing between buttons

    // Buttons on the left
    m_saveButton = new QPushButton("Save");
    m_saveButton->setToolTip("Save current settings to the selected profile");
    buttonDescLayout->addWidget(m_saveButton);

    m_saveAsButton = new QPushButton("Save As...");
    m_saveAsButton->setToolTip("Save current settings as a new profile");
    buttonDescLayout->addWidget(m_saveAsButton);

    m_loadButton = new QPushButton("Load");
    m_loadButton->setToolTip("Load the selected profile");
    buttonDescLayout->addWidget(m_loadButton);

    m_renameButton = new QPushButton("Rename...");
    m_renameButton->setToolTip("Rename the selected profile");
    buttonDescLayout->addWidget(m_renameButton);

    m_deleteButton = new QPushButton("Delete");
    m_deleteButton->setToolTip("Delete the selected profile");
    buttonDescLayout->addWidget(m_deleteButton);

    buttonDescLayout->addStretch(); // Prevent button expansion
    buttonDescLayout->addSpacing(10); // Spacing before description
    
    // Description on the right with last used below it - tighter spacing
    QVBoxLayout* rightInfoLayout = new QVBoxLayout();
    rightInfoLayout->setSpacing(2); // Minimal spacing between description and last used
    
    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setStyleSheet("font-style: italic; color: #666;");
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignRight);
    rightInfoLayout->addWidget(m_descriptionLabel);
    
    m_lastUsedLabel = new QLabel();
    m_lastUsedLabel->setStyleSheet("font-size: 11px; color: #888;");
    m_lastUsedLabel->setAlignment(Qt::AlignRight);
    rightInfoLayout->addWidget(m_lastUsedLabel);
    
    buttonDescLayout->addLayout(rightInfoLayout, 1); // Expand to fill space
    
    groupLayout->addLayout(buttonDescLayout);
}

void ProfileSelectionWidget::connectSignals()
{
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfileSelectionWidget::onProfileComboChanged);
    
    connect(m_saveButton, &QPushButton::clicked, this, &ProfileSelectionWidget::onSaveClicked);
    connect(m_saveAsButton, &QPushButton::clicked, this, &ProfileSelectionWidget::onSaveAsClicked);
    connect(m_loadButton, &QPushButton::clicked, this, &ProfileSelectionWidget::onLoadClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &ProfileSelectionWidget::onDeleteClicked);
    connect(m_renameButton, &QPushButton::clicked, this, &ProfileSelectionWidget::onRenameClicked);
    
    // Connect to profile manager signals
    if (m_profileManager) {
        connect(m_profileManager, &ConfigurationProfileManager::profileListChanged,
                this, &ProfileSelectionWidget::onProfileListChanged);
    }
}

void ProfileSelectionWidget::updateButtonStates()
{
    QString currentProfile = getCurrentProfileName();
    bool hasProfile = !currentProfile.isEmpty();
    bool isDefault = (currentProfile == "Default");
    bool profileExists = hasProfile && m_profileManager->profileExists(currentProfile);
    
    // Save button: enabled if profile exists (can overwrite existing profile)
    m_saveButton->setEnabled(profileExists);
    
    // Save As button: always enabled (saves current settings)
    m_saveAsButton->setEnabled(true);
    
    // Load button: enabled if profile exists
    m_loadButton->setEnabled(profileExists);
    
    // Delete button: enabled if not Default and profile exists
    m_deleteButton->setEnabled(profileExists && !isDefault);
    
    // Rename button: enabled if not Default and profile exists
    m_renameButton->setEnabled(profileExists && !isDefault);
}

bool ProfileSelectionWidget::confirmDeleteProfile(const QString& profileName)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Profile",
        QString("Are you sure you want to delete the profile '%1'?\n\nThis action cannot be undone.").arg(profileName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    return (reply == QMessageBox::Yes);
}

QString ProfileSelectionWidget::promptForProfileName(const QString& title, const QString& label, const QString& defaultText)
{
    bool ok;
    QString text = QInputDialog::getText(this, title, label, QLineEdit::Normal, defaultText, &ok);
    
    if (ok) {
        return text.trimmed();
    }
    
    return QString(); // User cancelled
}

void ProfileSelectionWidget::showErrorMessage(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
}

void ProfileSelectionWidget::showInfoMessage(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}