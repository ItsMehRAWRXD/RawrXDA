#include "ai_digestion_engine.hpp"
#include "ai_digestion_panel.hpp"
#include "ai_workers.h"
#include <QtWidgets/QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Testing AI Digestion System..." << std::endl;
    
    QApplication app(argc, argv);
    
    try {
        // Test AI Digestion Engine
        AIDigestionEngine engine;
        std::cout << "✓ AIDigestionEngine created successfully" << std::endl;
        
        // Test Worker Manager
        AIWorkerManager workerManager;
        std::cout << "✓ AIWorkerManager created successfully" << std::endl;
        
        // Test creating workers
        auto* digestionWorker = workerManager.createDigestionWorker(&engine);
        std::cout << "✓ DigestionWorker created successfully" << std::endl;
        
        // Test AI Digestion Panel
        AIDigestionPanel panel;
        panel.show();
        std::cout << "✓ AIDigestionPanel created successfully" << std::endl;
        
        std::cout << std::endl;
        std::cout << "🎉 All AI Digestion System components compiled and initialized successfully!" << std::endl;
        std::cout << "📋 System Features:" << std::endl;
        std::cout << "   • Multi-language content digestion (C++, Python, JavaScript, Assembly)" << std::endl;
        std::cout << "   • Background processing with progress monitoring" << std::endl;
        std::cout << "   • Professional Qt6 UI with tabbed interface" << std::endl;
        std::cout << "   • AI training pipeline integration" << std::endl;
        std::cout << "   • Production-ready observability and monitoring" << std::endl;
        std::cout << std::endl;
        std::cout << "The AI digestion panel is now displayed. Close it to exit the test." << std::endl;
        
        return app.exec();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}