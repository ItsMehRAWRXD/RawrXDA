// Main for minimal IDE test
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include "MainWindowMinimal.h"

int main(int argc, char* argv[])
{
    try {
        // Set up application attributes before QApplication creation
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
        
        QApplication app(argc, argv);
        app.setApplicationName("RawrXD-Minimal-IDE");
        app.setApplicationVersion("1.0.13");
        app.setOrganizationName("RawrXD");
        
        qDebug() << "Starting RawrXD Minimal IDE at" << QDateTime::currentDateTime();
        qDebug() << "Working directory:" << QDir::currentPath();
        
        qDebug() << "Creating MainWindow...";
        MainWindowMinimal window;
        
        qDebug() << "Showing window...";
        window.show();
        
        qDebug() << "Entering event loop...";
        int result = app.exec();
        
        qDebug() << "Application exiting with code:" << result;
        return result;
    }
    catch (const std::exception& e) {
        qCritical() << "Exception caught in main:" << e.what();
        
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Exception in main(): " << e.what() << "\n";
            stream << "Time: " << QDateTime::currentDateTime().toString() << "\n";
            errorLog.close();
        }
        return -1;
    }
    catch (...) {
        qCritical() << "Unknown exception caught in main";
        
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Unknown exception in main()\n";
            stream << "Time: " << QDateTime::currentDateTime().toString() << "\n";
            errorLog.close();
        }
        return -3;
    }
}