/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifdef _WIN32
#include "windows_compat.h"
#endif

#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Fujisan");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("8bitrelics");

#ifdef Q_OS_WIN
    // On Windows with cross-compiled MinGW Qt builds, the registry (NativeFormat)
    // can be unreliable. Use an INI file in AppData/Roaming/8bitrelics/Fujisan/ instead,
    // which is always readable and writable for the current user.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    qDebug() << "Windows settings directory:" << settingsDir;
#endif

    MainWindow window;
    window.show();
    
    return app.exec();
}