/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef CONFIGURATIONPROFILEMANAGER_H
#define CONFIGURATIONPROFILEMANAGER_H

#include <QObject>
#include <QStringList>
#include "configurationprofile.h"

class ConfigurationProfileManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurationProfileManager(QObject *parent = nullptr);
    
    // Profile management
    QStringList getProfileNames() const;
    ConfigurationProfile loadProfile(const QString& name);
    bool saveProfile(const QString& name, const ConfigurationProfile& profile);
    bool deleteProfile(const QString& name);
    bool renameProfile(const QString& oldName, const QString& newName);
    
    // Current profile tracking
    QString getCurrentProfileName() const;
    void setCurrentProfileName(const QString& name);
    
    // Profile information
    bool profileExists(const QString& name) const;
    int getProfileCount() const;
    QDateTime getProfileLastUsed(const QString& name) const;
    QString getProfileDescription(const QString& name) const;
    
    // Profile creation helpers
    QString generateUniqueProfileName(const QString& baseName = "New Profile") const;
    ConfigurationProfile createDefaultProfile(const QString& name, const QString& description = "") const;
    
    // Validation
    bool isValidProfileName(const QString& name) const;
    QString sanitizeProfileName(const QString& name) const;
    
    // Default profiles
    void createDefaultProfilesIfNeeded();
    
signals:
    void profileChanged(const QString& profileName);
    void profileListChanged();
    void profileSaved(const QString& profileName);
    void profileDeleted(const QString& profileName);
    void profileRenamed(const QString& oldName, const QString& newName);

public slots:
    void refreshProfileList();

private:
    QString m_currentProfileName;
    QStringList m_cachedProfileList;
    
    void updateProfileCache();
    bool validateProfile(const ConfigurationProfile& profile) const;
};

#endif // CONFIGURATIONPROFILEMANAGER_H