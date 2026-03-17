// RawrXD Agentic IDE
// Advanced AI-powered IDE with terminal integration and agentic capabilities

#include <QApplication>
#include <QDebug>
#include <iostream>
#include "agentic_ide.h"

int main(int argc, char* argv[]) {
    std::cout << "[MAIN] Starting QApplication" << std::endl;
    std::cout.flush();
    
    // Create Qt application
    QApplication app(argc, argv);
    std::cout << "[MAIN] QApplication created" << std::endl;
    std::cout.flush();
    
    try {
        // Create and show the Agentic IDE window
        std::cout << "[MAIN] Creating AgenticIDE window" << std::endl;
        std::cout.flush();
        
        AgenticIDE *ide = new AgenticIDE();
        
        std::cout << "[MAIN] IDE created, showing window" << std::endl;
        std::cout.flush();
        
        ide->show();
        
        std::cout << "[MAIN] Window shown, entering event loop" << std::endl;
        std::cout.flush();
        
        qDebug() << "RawrXD Agentic IDE started successfully";
        
        // Run the Qt event loop
        int result = app.exec();
        
        std::cout << "[MAIN] Event loop exited with code" << result << std::endl;
        std::cout.flush();
        
        delete ide;
        return result;
    }
    catch (const std::exception& e) {
        std::cout << "[ERROR] Exception: " << e.what() << std::endl;
        std::cout.flush();
        qCritical() << "Fatal error:" << e.what();
        return 1;
    }
    catch (...) {
        std::cout << "[ERROR] Unknown exception" << std::endl;
        std::cout.flush();
        qCritical() << "Unknown fatal error occurred";
        return 1;
    }
}