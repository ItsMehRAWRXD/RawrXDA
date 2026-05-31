// Minimal agentic_ide_test.cpp - smoke test for agentic IDE header compilation

#include "agentic_ide.h"
#include <iostream>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    std::cout << "[agentic_ide_test] Header compilation OK\n";
    
    // Verify AgenticIDE singleton can be instantiated
    auto& ide = AgenticIDE::instance();
    (void)ide;
    
    std::cout << "[agentic_ide_test] Singleton access OK\n";
    std::cout << "[agentic_ide_test] PASS\n";
    
    return 0;
}

