// RawrXD IDE - C++ Migration from PowerShell
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
// #include "auto_update.hpp"
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    try {
        QApplication app(argc, argv);
        
        qDebug() << "Starting RawrXD-QtShell...";
        
        // Disable auto-update during initial testing
        // AutoUpdate updater;
        // updater.checkAndInstall();
        
        qDebug() << "Creating MainWindow...";
        MainWindow window;
        qDebug() << "Showing window...";
        window.show();
        
        qDebug() << "Entering event loop...";
        return app.exec();
    }
    catch (const std::exception& e) {
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errorLog.write(e.what());
            errorLog.close();
        }
        return -1;
    }
}
