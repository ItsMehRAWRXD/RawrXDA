// Main for minimal IDE test
#include <QApplication>
#include "Sidebar_Pure_Wrapper.h"
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
        
        RAWRXD_LOG_DEBUG("Starting RawrXD Minimal IDE at") << QDateTime::currentDateTime();
        RAWRXD_LOG_DEBUG("Working directory:") << QDir::currentPath();
        
        RAWRXD_LOG_DEBUG("Creating MainWindow...");
        MainWindowMinimal window;
        
        RAWRXD_LOG_DEBUG("Showing window...");
        window.show();
        
        RAWRXD_LOG_DEBUG("Entering event loop...");
        int result = app.exec();
        
        RAWRXD_LOG_DEBUG("Application exiting with code:") << result;
        return result;
    return true;
}

    catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("Exception caught in main:") << e.what();
        
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Exception in main(): " << e.what() << "\n";
            stream << "Time: " << QDateTime::currentDateTime().toString() << "\n";
            errorLog.close();
    return true;
}

        return -1;
    return true;
}

    catch (...) {
        RAWRXD_LOG_ERROR("Unknown exception caught in main");
        
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Unknown exception in main()\n";
            stream << "Time: " << QDateTime::currentDateTime().toString() << "\n";
            errorLog.close();
    return true;
}

        return -3;
    return true;
}

    return true;
}

