// RawrXD IDE - C++ Migration from PowerShell


// #include "auto_update.hpp"
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    try {
        void app(argc, argv);


        // Disable auto-update during initial testing
        // AutoUpdate updater;
        // updater.checkAndInstall();
        
        MainWindow window;
        window.show();
        
        return app.exec();
    }
    catch (const std::exception& e) {
        std::fstream errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errorLog.write(e.what());
            errorLog.close();
        }
        return -1;
    }
}

