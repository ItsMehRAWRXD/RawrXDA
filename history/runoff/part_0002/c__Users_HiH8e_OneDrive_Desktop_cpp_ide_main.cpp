#include "ide.hpp"
#include <iostream>

int main() {
    // Create the main IDE window
    MainWindow ide;
    
    // Initialize the application
    if (!ide.initialize()) {
        std::cout << "Failed to initialize IDE!" << std::endl;
        return -1;
    }
    
    // Run the main application loop
    ide.run();
    
    // Clean shutdown
    ide.shutdown();
    
    return 0;
}