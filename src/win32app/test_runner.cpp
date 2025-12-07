// IDE Test Runner - Comprehensive function testing with logging
// Tests every aspect of the RawrXD IDE

#include "Win32IDE.h"
#include "IDETestAgent.h"
#include "IDELogger.h"
#include <windows.h>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    std::cout << "Test runner entry point reached\n";
    std::ofstream test("test_file_write.txt");
    test << "File write test successful" << std::endl;
    test.close();
    std::cout << "RawrXD IDE Test Runner\n";
    std::cout << "======================\n\n";

    // Initialize logging to a specific test log file
    std::cout << "Step 1: Initializing logger...\n";
    IDELogger::getInstance().initialize();
    std::cout << "Step 2: Logger initialized\n";
    LOG_INFO("Test runner started");
    std::cout << "Step 3: Log message sent\n";

    try {
        // Create IDE instance
        HINSTANCE hInstance = GetModuleHandle(NULL);
        Win32IDE ide(hInstance);
        LOG_INFO("IDE instance created");

        // Create main window (but don't show it if in automated mode)
        if (!ide.createWindow()) {
            LOG_ERROR("Failed to create IDE window");
            std::cerr << "ERROR: Failed to create IDE window\n";
            return 1;
        }
        LOG_INFO("IDE window created");

        // Show window if not in headless mode
        bool headless = false;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--headless") {
                headless = true;
                break;
            }
        }

        if (!headless) {
            ide.showWindow();
            LOG_INFO("IDE window shown");
        } else {
            LOG_INFO("Running in headless mode");
        }

        // Give the window time to fully initialize
        Sleep(500);
        
        // Create test agent
        IDETestAgent testAgent(&ide);
        LOG_INFO("Test agent created");

        // Run all tests
        std::cout << "Running comprehensive IDE tests...\n\n";
        testAgent.runAllTests();

        // Get results
        const auto& results = testAgent.getResults();
        
        // Print results to console
        std::cout << "\n===========================================\n";
        std::cout << "Test Results Summary\n";
        std::cout << "===========================================\n";
        
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) {
                passed++;
                std::cout << "✓ " << result.testName << " (" << result.durationMs << "ms)\n";
            } else {
                failed++;
                std::cout << "✗ " << result.testName << " - " << result.errorMessage << "\n";
            }
        }
        
        std::cout << "\nTotal: " << results.size() << " tests\n";
        std::cout << "Passed: " << passed << " (" << (results.size() > 0 ? (passed * 100 / results.size()) : 0) << "%)\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "===========================================\n";

        // Write detailed results to file
        std::ofstream resultFile("C:\\RawrXD_IDE_TestResults.txt");
        if (resultFile.is_open()) {
            resultFile << "RawrXD IDE Test Results\n";
            resultFile << "=======================\n\n";
            resultFile << "Test Run Date: " << __DATE__ << " " << __TIME__ << "\n\n";
            
            for (const auto& result : results) {
                resultFile << (result.passed ? "[PASS] " : "[FAIL] ");
                resultFile << result.testName;
                resultFile << " (" << result.durationMs << "ms)";
                if (!result.passed) {
                    resultFile << "\n  Error: " << result.errorMessage;
                }
                resultFile << "\n";
            }
            
            resultFile << "\nSummary:\n";
            resultFile << "Total: " << results.size() << "\n";
            resultFile << "Passed: " << passed << "\n";
            resultFile << "Failed: " << failed << "\n";
            resultFile.close();
            
            std::cout << "\nDetailed results written to: C:\\RawrXD_IDE_TestResults.txt\n";
            LOG_INFO("Test results written to file");
        }

        std::cout << "Log file: C:\\RawrXD_IDE_TestRun.log\n";
        
        LOG_INFO("Test runner completed");
        
        // If not headless, keep window open for manual inspection
        if (!headless) {
            std::cout << "\nPress Enter to close IDE and exit...\n";
            std::cin.get();
        }

        return (failed == 0) ? 0 : 1;

    } catch (const std::exception& e) {
        LOG_CRITICAL("Test runner exception: " + std::string(e.what()));
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        return 2;
    } catch (...) {
        LOG_CRITICAL("Unknown test runner exception");
        std::cerr << "CRITICAL ERROR: Unknown exception\n";
        return 2;
    }
}
