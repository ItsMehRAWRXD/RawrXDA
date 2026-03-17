#include "native_ide_manager.hpp"
#include <iostream>

int main() {
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  🚀 NATIVE IDE MANAGER - UNIFIED CURSOR INTEGRATION\n";
    std::cout << "═══════════════════════════════════════════════════════════\n\n";
    
    try {
        ServiceManager manager;
        
        std::cout << "Starting unified native IDE services...\n";
        manager.startAllServices();
        
        std::cout << "\n✅ All services running! Cursor integration ready.\n";
        std::cout << "Services available:\n";
        std::cout << "  • BigDaddyG Model Server: http://localhost:11440\n";
        std::cout << "  • Cursor WebSocket Bridge: http://localhost:8081\n";
        std::cout << "  • Universal Compiler: http://localhost:8080\n\n";
        
        // Wait for shutdown signal
        manager.waitForShutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n✅ Shutdown complete. All services stopped cleanly.\n";
    return 0;
}