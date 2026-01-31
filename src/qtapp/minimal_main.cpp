// Minimal main for testing basic Qt functionality


#include "MinimalWindow.h"

int main(int argc, char* argv[])
{
    try {
        
        QApplication app(argc, argv);
        app.setApplicationName("RawrXD-Minimal");
        app.setApplicationVersion("1.0.0");
        
        MinimalWindow window;
        
        window.show();
        
        return app.exec();
        
    } catch (const std::exception& e) {
        return -1;
    } catch (...) {
        return -2;
    }
}

