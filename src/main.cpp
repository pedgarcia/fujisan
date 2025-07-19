/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 * 
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include <QApplication>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Fujisan");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Atari800 Project");
    
    qDebug() << "Starting Fujisan...";
    
    MainWindow window;
    window.show();
    
    return app.exec();
}