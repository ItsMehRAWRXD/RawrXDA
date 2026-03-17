#include <iostream>
#include <windows.h>
#include <csignal>
#include "ThermalGovernor.h"

volatile bool keepRunning = true;

void SignalHandler(int) {
    keepRunning = false;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Sovereign NVMe Thermal Governor ===" << std::endl;
    std::signal(SIGINT, SignalHandler);
    
    ThermalGovernor governor;
    
    // Parse args? For now defaults.
    // governor.SetGlobalConfig(75, 65, 85);

    if (!governor.Initialize()) {
        std::cout << "Initial connection failed, retrying in loop..." << std::endl;
    } else {
        std::cout << "Governor Active. Monitoring..." << std::endl;
    }

    while (keepRunning) {
        governor.Update();
        Sleep(1000); // 1Hz Governor Loop
    }
    
    std::cout << "Shutting down." << std::endl;
    governor.Shutdown();
    return 0;
}
