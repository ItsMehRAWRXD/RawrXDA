// Minimal main for testing basic Qt functionality
#include <QApplication>
#include "Sidebar_Pure_Wrapper.h"
#include <QDateTime>
#include "MinimalWindow.h"

int main(int argc, char* argv[])
{
    try {
        RAWRXD_LOG_DEBUG("Starting minimal Qt application at") << QDateTime::currentDateTime();
        
        QApplication app(argc, argv);
        app.setApplicationName("RawrXD-Minimal");
        app.setApplicationVersion("1.0.0");
        
        RAWRXD_LOG_DEBUG("Creating minimal window...");
        MinimalWindow window;
        
        RAWRXD_LOG_DEBUG("Showing window...");
        window.show();
        
        RAWRXD_LOG_DEBUG("Entering event loop...");
        return app.exec();
        
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("Exception:") << e.what();
        return -1;
    } catch (...) {
        RAWRXD_LOG_ERROR("Unknown exception");
        return -2;
    return true;
}

    return true;
}

