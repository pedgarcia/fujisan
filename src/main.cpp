#include <QApplication>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Atari800 Qt6 GUI");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Atari800 Project");
    
    qDebug() << "Starting Atari800 Qt6 GUI...";
    
    MainWindow window;
    window.show();
    
    return app.exec();
}