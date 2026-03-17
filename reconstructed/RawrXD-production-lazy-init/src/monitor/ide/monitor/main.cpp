// RawrXD IDE Health Monitor - Main CLI executable
// Real-time status dashboard for IDE features
#include "ide_health_monitor.hpp"
#include "feature_probes.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <cstring>

using namespace RawrXD::Monitor;

void printHeader() {
    ConsoleColor::setCyan();
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                  RawrXD IDE Health Monitor                      ║\n";
    std::cout << "║              Real-time Feature Status Dashboard                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    ConsoleColor::reset();
}

void printUsage() {
    std::cout << "\nUsage: ide_monitor.exe [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --once              Run diagnostics once and exit\n";
    std::cout << "  --continuous [N]    Monitor continuously every N seconds (default: 5)\n";
    std::cout << "  --json              Output as JSON\n";
    std::cout << "  --log <file>        Save results to file\n";
    std::cout << "  --help              Show this help message\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  ide_monitor.exe --once                    # Run once\n";
    std::cout << "  ide_monitor.exe --continuous 10           # Monitor every 10 seconds\n";
    std::cout << "  ide_monitor.exe --continuous 5 --json     # JSON output, every 5 seconds\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    // Parse arguments
    bool runOnce = true;
    bool jsonOutput = false;
    std::string logFile;
    int interval = 5;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--once") {
            runOnce = true;
        } else if (arg == "--continuous") {
            runOnce = false;
            if (i + 1 < argc) {
                interval = std::atoi(argv[++i]);
            }
        } else if (arg == "--json") {
            jsonOutput = true;
        } else if (arg == "--log") {
            if (i + 1 < argc) {
                logFile = argv[++i];
            }
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }
    
    // Initialize health monitor
    printHeader();
    
    auto monitor = std::make_unique<IDEHealthMonitor>();
    
    // Register all probes
    std::cout << "Registering feature probes...\n";
    monitor->registerProbe(std::make_shared<AIChatProbe>());
    monitor->registerProbe(std::make_shared<TerminalsProbe>());
    monitor->registerProbe(std::make_shared<FileBrowserProbe>());
    monitor->registerProbe(std::make_shared<TODOProbe>());
    monitor->registerProbe(std::make_shared<AgenticEngineProbe>());
    monitor->registerProbe(std::make_shared<InferenceEngineProbe>());
    monitor->registerProbe(std::make_shared<LSPClientProbe>());
    monitor->registerProbe(std::make_shared<ReferenceWidgetsProbe>());
    
    std::cout << "8 feature probes registered.\n";
    
    if (runOnce) {
        // Single run mode
        std::cout << "\nRunning diagnostic report...\n";
        monitor->runFullDiagnostics();
        
        if (jsonOutput) {
            std::cout << "\nJSON Export:\n";
            std::cout << monitor->exportAsJSON();
        }
        
        if (!logFile.empty()) {
            monitor->logToFile(logFile);
            std::cout << "\nResults saved to: " << logFile << "\n";
        }
    } else {
        // Continuous monitoring mode
        std::cout << "\nStarting continuous monitoring (interval: " << interval << "s)...\n";
        std::cout << "Press Ctrl+C to stop.\n\n";
        
        monitor->startContinuousMonitoring(interval);
        
        // Display status periodically
        while (true) {
            monitor->displayStatus();
            
            if (jsonOutput) {
                std::cout << "JSON Export:\n";
                std::cout << monitor->exportAsJSON();
            }
            
            std::cout << "Next update in " << interval << " seconds. Press Ctrl+C to stop.\n";
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }
    
    std::cout << "\nMonitor exiting.\n";
    return 0;
}
