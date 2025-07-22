/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "configurationprofilemanager.h"
#include <QDebug>
#include <QDateTime>

ConfigurationProfileManager::ConfigurationProfileManager(QObject *parent)
    : QObject(parent)
    , m_currentProfileName("Default")
{
    updateProfileCache();
    createDefaultProfilesIfNeeded();
}

QStringList ConfigurationProfileManager::getProfileNames() const
{
    return m_cachedProfileList;
}

ConfigurationProfile ConfigurationProfileManager::loadProfile(const QString& name)
{
    if (!isValidProfileName(name) || !profileExists(name)) {
        qWarning() << "Cannot load invalid or non-existent profile:" << name;
        return ConfigurationProfile();
    }
    
    ConfigurationProfile profile = ProfileStorage::loadProfileFromFile(name);
    
    if (profile.isValid()) {
        // Update last used time
        profile.updateLastUsed();
        ProfileStorage::saveProfileToFile(name, profile);
        
        qDebug() << "Profile loaded successfully:" << name;
    } else {
        qWarning() << "Failed to load profile:" << name;
    }
    
    return profile;
}

bool ConfigurationProfileManager::saveProfile(const QString& name, const ConfigurationProfile& profile)
{
    if (!isValidProfileName(name) || !validateProfile(profile)) {
        qWarning() << "Cannot save invalid profile:" << name;
        return false;
    }
    
    // Make a copy of the profile to set metadata
    ConfigurationProfile profileToSave = profile;
    profileToSave.name = name;
    profileToSave.updateLastUsed();
    
    // If this is a new profile, set creation time
    if (!profileExists(name)) {
        profileToSave.created = QDateTime::currentDateTime();
    } else {
        // Preserve original creation time
        ConfigurationProfile existingProfile = ProfileStorage::loadProfileFromFile(name);
        if (existingProfile.isValid()) {
            profileToSave.created = existingProfile.created;
        }
    }
    
    bool success = ProfileStorage::saveProfileToFile(name, profileToSave);
    
    if (success) {
        updateProfileCache();
        emit profileSaved(name);
        emit profileListChanged();
        qDebug() << "Profile saved successfully:" << name;
    } else {
        qWarning() << "Failed to save profile:" << name;
    }
    
    return success;
}

bool ConfigurationProfileManager::deleteProfile(const QString& name)
{
    if (!isValidProfileName(name) || !profileExists(name)) {
        qWarning() << "Cannot delete invalid or non-existent profile:" << name;
        return false;
    }
    
    // Don't allow deleting the default profile
    if (name == "Default") {
        qWarning() << "Cannot delete the Default profile";
        return false;
    }
    
    bool success = ProfileStorage::deleteProfileFile(name);
    
    if (success) {
        // If we're deleting the current profile, switch to Default
        if (m_currentProfileName == name) {
            setCurrentProfileName("Default");
        }
        
        updateProfileCache();
        emit profileDeleted(name);
        emit profileListChanged();
        qDebug() << "Profile deleted successfully:" << name;
    } else {
        qWarning() << "Failed to delete profile:" << name;
    }
    
    return success;
}

bool ConfigurationProfileManager::renameProfile(const QString& oldName, const QString& newName)
{
    if (!isValidProfileName(oldName) || !isValidProfileName(newName)) {
        qWarning() << "Invalid profile names for rename:" << oldName << "to" << newName;
        return false;
    }
    
    if (!profileExists(oldName)) {
        qWarning() << "Source profile does not exist:" << oldName;
        return false;
    }
    
    if (profileExists(newName)) {
        qWarning() << "Target profile already exists:" << newName;
        return false;
    }
    
    // Don't allow renaming the default profile
    if (oldName == "Default") {
        qWarning() << "Cannot rename the Default profile";
        return false;
    }
    
    // Load the profile
    ConfigurationProfile profile = ProfileStorage::loadProfileFromFile(oldName);
    if (!profile.isValid()) {
        qWarning() << "Failed to load profile for rename:" << oldName;
        return false;
    }
    
    // Save with new name
    profile.name = newName;
    bool saveSuccess = ProfileStorage::saveProfileToFile(newName, profile);
    
    if (!saveSuccess) {
        qWarning() << "Failed to save renamed profile:" << newName;
        return false;
    }
    
    // Delete old profile
    bool deleteSuccess = ProfileStorage::deleteProfileFile(oldName);
    
    if (!deleteSuccess) {
        qWarning() << "Failed to delete old profile after rename:" << oldName;
        // Try to cleanup the new profile
        ProfileStorage::deleteProfileFile(newName);
        return false;
    }
    
    // Update current profile name if needed
    if (m_currentProfileName == oldName) {
        m_currentProfileName = newName;
        emit profileChanged(newName);
    }
    
    updateProfileCache();
    emit profileRenamed(oldName, newName);
    emit profileListChanged();
    qDebug() << "Profile renamed successfully:" << oldName << "to" << newName;
    
    return true;
}

QString ConfigurationProfileManager::getCurrentProfileName() const
{
    return m_currentProfileName;
}

void ConfigurationProfileManager::setCurrentProfileName(const QString& name)
{
    if (m_currentProfileName != name) {
        m_currentProfileName = name;
        emit profileChanged(name);
        qDebug() << "Current profile changed to:" << name;
    }
}

bool ConfigurationProfileManager::profileExists(const QString& name) const
{
    return ProfileStorage::profileFileExists(name);
}

int ConfigurationProfileManager::getProfileCount() const
{
    return m_cachedProfileList.size();
}

QDateTime ConfigurationProfileManager::getProfileLastUsed(const QString& name) const
{
    if (!profileExists(name)) {
        return QDateTime();
    }
    
    ConfigurationProfile profile = ProfileStorage::loadProfileFromFile(name);
    return profile.lastUsed;
}

QString ConfigurationProfileManager::getProfileDescription(const QString& name) const
{
    if (!profileExists(name)) {
        return QString();
    }
    
    ConfigurationProfile profile = ProfileStorage::loadProfileFromFile(name);
    return profile.description;
}

QString ConfigurationProfileManager::generateUniqueProfileName(const QString& baseName) const
{
    QString name = baseName;
    int counter = 1;
    
    while (profileExists(name)) {
        name = QString("%1 %2").arg(baseName).arg(counter);
        counter++;
    }
    
    return name;
}

ConfigurationProfile ConfigurationProfileManager::createDefaultProfile(const QString& name, const QString& description) const
{
    ConfigurationProfile profile;
    profile.name = name;
    profile.description = description;
    profile.created = QDateTime::currentDateTime();
    profile.lastUsed = profile.created;
    
    // Use default values from struct initialization
    return profile;
}

bool ConfigurationProfileManager::isValidProfileName(const QString& name) const
{
    return ProfileStorage::isValidProfileName(name);
}

QString ConfigurationProfileManager::sanitizeProfileName(const QString& name) const
{
    return ProfileStorage::sanitizeProfileName(name);
}

void ConfigurationProfileManager::createDefaultProfilesIfNeeded()
{
    // Create Default profile if it doesn't exist
    if (!profileExists("Default")) {
        ConfigurationProfile defaultProfile = createDefaultProfile("Default", "Default Fujisan configuration");
        saveProfile("Default", defaultProfile);
        qDebug() << "Created Default profile";
    }
    
    // Create example Gaming profile if it doesn't exist
    if (!profileExists("Gaming Setup")) {
        ConfigurationProfile gamingProfile = createGamingProfile();
        saveProfile("Gaming Setup", gamingProfile);
        qDebug() << "Created Gaming Setup profile";
    }
    
    // Create example Development profile if it doesn't exist
    if (!profileExists("Development")) {
        ConfigurationProfile devProfile = createDevelopmentProfile();
        saveProfile("Development", devProfile);
        qDebug() << "Created Development profile";
    }
}

ConfigurationProfile ConfigurationProfileManager::createGamingProfile() const
{
    ConfigurationProfile profile = createDefaultProfile("Gaming Setup", "Optimized for gaming");
    
    // Gaming-optimized settings
    profile.machineType = "-xl";           // Popular gaming machine
    profile.videoSystem = "-ntsc";         // Common for games
    profile.basicEnabled = false;          // Games usually don't need BASIC
    profile.altirraOSEnabled = true;       // Use built-in OS
    
    // Performance settings for gaming
    profile.turboMode = false;             // Normal speed for authentic feel
    profile.emulationSpeedIndex = 1;       // 1x speed
    
    // Audio settings
    profile.audioEnabled = true;
    profile.audioVolume = 90;              // Higher volume for gaming
    profile.consoleSound = true;           // Enable all sounds
    profile.serialSound = true;
    
    // Video settings
    profile.artifactingMode = "ntsc-new";  // NTSC artifacts for authentic look
    profile.scalingFilter = true;
    profile.keepAspectRatio = true;
    
    return profile;
}

ConfigurationProfile ConfigurationProfileManager::createDevelopmentProfile() const
{
    ConfigurationProfile profile = createDefaultProfile("Development", "Setup for programming and development");
    
    // Development-oriented settings
    profile.machineType = "-xe";           // 130XE for more memory
    profile.videoSystem = "-pal";          // PAL for development
    profile.basicEnabled = true;           // Enable BASIC for programming
    profile.altirraOSEnabled = true;       // Use built-in OS
    
    // Memory expansions for development
    profile.enableMapRam = true;
    
    // Performance settings
    profile.turboMode = false;             // Normal speed
    profile.emulationSpeedIndex = 1;       // 1x speed
    
    // Audio settings - quieter for concentration
    profile.audioEnabled = true;
    profile.audioVolume = 50;              // Lower volume
    profile.consoleSound = false;          // Disable keyboard clicks
    
    // Video settings
    profile.artifactingMode = "none";      // Clean display for text
    profile.showFPS = true;                // Show performance info
    
    // Enable SIO acceleration for faster disk access
    profile.sioAcceleration = true;
    
    return profile;
}

void ConfigurationProfileManager::refreshProfileList()
{
    updateProfileCache();
    emit profileListChanged();
}

void ConfigurationProfileManager::updateProfileCache()
{
    m_cachedProfileList = ProfileStorage::getAvailableProfiles();
    
    // Ensure Default is always first in the list
    if (m_cachedProfileList.contains("Default")) {
        m_cachedProfileList.removeAll("Default");
        m_cachedProfileList.prepend("Default");
    }
    
    qDebug() << "Profile cache updated:" << m_cachedProfileList.size() << "profiles found";
}

bool ConfigurationProfileManager::validateProfile(const ConfigurationProfile& profile) const
{
    // Basic validation
    if (!profile.isValid()) {
        return false;
    }
    
    // Check that required fields are reasonable
    if (profile.audioFrequency < 8000 || profile.audioFrequency > 48000) {
        qWarning() << "Invalid audio frequency:" << profile.audioFrequency;
        return false;
    }
    
    if (profile.audioBits != 8 && profile.audioBits != 16) {
        qWarning() << "Invalid audio bit depth:" << profile.audioBits;
        return false;
    }
    
    if (profile.audioVolume < 0 || profile.audioVolume > 100) {
        qWarning() << "Invalid audio volume:" << profile.audioVolume;
        return false;
    }
    
    if (profile.emulationSpeedIndex < 0 || profile.emulationSpeedIndex > 10) {
        qWarning() << "Invalid emulation speed index:" << profile.emulationSpeedIndex;
        return false;
    }
    
    // Add more validation as needed
    return true;
}