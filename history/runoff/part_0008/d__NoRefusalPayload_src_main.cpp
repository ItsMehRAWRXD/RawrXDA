#include "PayloadSupervisor.hpp"
#include <windows.h>
#include <iostream>
#include <vector>

/**
 * @brief Entry point for the No-Refusal Payload Engine.
 * 
 * This C++ main() function:
 * 1. Initializes the PayloadSupervisor
 * 2. Applies process hardening and anti-debugging measures
 * 3. Protects critical memory regions
 * 4. Transfers execution to the MASM No-Refusal core
 */

int main() {
    try {
        // Banner
        std::cout << "========================================\n";
        std::cout << "--- No-Refusal IDE Environment Init ---\n";
        std::cout << "========================================\n\n";

        // Create supervisor instance
        Engine::PayloadSupervisor supervisor;

        std::cout << "[*] Initializing hardening mechanisms...\n";
        if (!supervisor.InitializeHardening()) {
            std::cerr << "[!] Hardening initialization failed.\n";
            return 1;
        }

        std::cout << "[*] Protecting memory regions...\n";
        supervisor.ProtectMemoryRegions();

        std::cout << "[*] Launching payload core...\n";
        supervisor.LaunchCore();

        // If we reach here, the ASM core has returned (should be rare)
        std::cout << "[!] ASM core returned unexpectedly.\n";
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "[!] Exception in main: " << ex.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "[!] Unknown exception in main.\n";
        return 1;
    }
}
