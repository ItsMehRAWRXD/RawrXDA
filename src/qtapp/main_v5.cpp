// RawrXD Agentic IDE - v5.0 Entry Point
#include "MainWindow_v5.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    qDebug() << "[Main] Starting RawrXD Agentic IDE v5.0";
    
    QApplication app(argc, argv);
    
    qDebug() << "[Main] QApplication created, showing MainWindow";
    
    RawrXD::MainWindow mainWindow;
    mainWindow.show();
    
    qDebug() << "[Main] Entering event loop";
    
    return app.exec();
}
