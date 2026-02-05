// Minimal main for testing basic Qt functionality
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include "MinimalWindow.h"

int main(int argc, char* argv[])
{
    try {
        qDebug() << "Starting minimal Qt application at" << QDateTime::currentDateTime();
        
        QApplication app(argc, argv);
        app.setApplicationName("RawrXD-Minimal");
        app.setApplicationVersion("1.0.0");
        
        qDebug() << "Creating minimal window...";
        MinimalWindow window;
        
        qDebug() << "Showing window...";
        window.show();
        
        qDebug() << "Entering event loop...";
        return app.exec();
        
    } catch (const std::exception& e) {
        qCritical() << "Exception:" << e.what();
        return -1;
    } catch (...) {
        qCritical() << "Unknown exception";
        return -2;
    }
}