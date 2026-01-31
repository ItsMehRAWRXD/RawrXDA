// Main for minimal IDE test


#include "MainWindowMinimal.h"

int main(int argc, char* argv[])
{
    try {
        // Set up application attributes before void creation
        void::setAttribute(//AA_EnableHighDpiScaling);
        void::setAttribute(//AA_UseHighDpiPixmaps);
        
        void app(argc, argv);
        app.setApplicationName("RawrXD-Minimal-IDE");
        app.setApplicationVersion("1.0.13");
        app.setOrganizationName("RawrXD");


        MainWindowMinimal window;
        
        window.show();
        
        int result = app.exec();
        
        return result;
    }
    catch (const std::exception& e) {
        
        std::fstream errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Exception in main(): " << e.what() << "\n";
            stream << "Time: " << std::chrono::system_clock::time_point::currentDateTime().toString() << "\n";
            errorLog.close();
        }
        return -1;
    }
    catch (...) {
        
        std::fstream errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&errorLog);
            stream << "Unknown exception in main()\n";
            stream << "Time: " << std::chrono::system_clock::time_point::currentDateTime().toString() << "\n";
            errorLog.close();
        }
        return -3;
    }
}

