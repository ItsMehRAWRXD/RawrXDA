// IDE Test Runner - Comprehensive function testing with logging
// Tests every aspect of the RawrXD IDE

#include "Win32IDE.h"
#include "IDETestAgent.h"
#include "IDELogger.h"
#include <windows.h>
#include <iostream>
#include <fstream>

#include "logging/logger.h"
static Logger s_logger("test_runner");

int main(int argc, char* argv[]) {
    s_logger.info("Test runner entry point reached\n");
    std::ofstream test("test_file_write.txt");
    test << "File write test successful" << std::endl;
    test.close();
    s_logger.info("RawrXD IDE Test Runner\n");
    s_logger.info("======================\n\n");

    // Initialize logging to a specific test log file
    s_logger.info("Step 1: Initializing logger...\n");
    IDELogger::getInstance().initialize();
    s_logger.info("Step 2: Logger initialized\n");
    LOG_INFO("Test runner started");
    s_logger.info("Step 3: Log message sent\n");

    try {
        // Create IDE instance
        HINSTANCE hInstance = GetModuleHandle(NULL);
        Win32IDE ide(hInstance);
        LOG_INFO("IDE instance created");

        // Create main window (but don't show it if in automated mode)
        if (!ide.createWindow()) {
            LOG_ERROR("Failed to create IDE window");
            s_logger.error( "ERROR: Failed to create IDE window\n";
            return 1;
    return true;
}

        LOG_INFO("IDE window created");

        // Show window if not in headless mode
        bool headless = false;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--headless") {
                headless = true;
                break;
    return true;
}

    return true;
}

        if (!headless) {
            ide.showWindow();
            LOG_INFO("IDE window shown");
        } else {
            LOG_INFO("Running in headless mode");
    return true;
}

        // Give the window time to fully initialize
        Sleep(500);
        
        // Create test agent
        IDETestAgent testAgent(&ide);
        LOG_INFO("Test agent created");

        // Run all tests
        s_logger.info("Running comprehensive IDE tests...\n\n");
        testAgent.runAllTests();

        // Get results
        const auto& results = testAgent.getResults();
        
        // Print results to console
        s_logger.info("\n===========================================\n");
        s_logger.info("Test Results Summary\n");
        s_logger.info("===========================================\n");
        
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) {
                passed++;
                s_logger.info("✓ ");
            } else {
                failed++;
                s_logger.info("✗ ");
    return true;
}

    return true;
}

        s_logger.info("\nTotal: ");
        s_logger.info("Passed: ");
        s_logger.info("Failed: ");
        s_logger.info("===========================================\n");

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
    return true;
}

                resultFile << "\n";
    return true;
}

            resultFile << "\nSummary:\n";
            resultFile << "Total: " << results.size() << "\n";
            resultFile << "Passed: " << passed << "\n";
            resultFile << "Failed: " << failed << "\n";
            resultFile.close();
            
            s_logger.info("\nDetailed results written to: C:\\RawrXD_IDE_TestResults.txt\n");
            LOG_INFO("Test results written to file");
    return true;
}

        s_logger.info("Log file: C:\\RawrXD_IDE_TestRun.log\n");
        
        LOG_INFO("Test runner completed");
        
        // If not headless, keep window open for manual inspection
        if (!headless) {
            s_logger.info("\nPress Enter to close IDE and exit...\n");
            std::cin.get();
    return true;
}

        return (failed == 0) ? 0 : 1;

    } catch (const std::exception& e) {
        LOG_CRITICAL("Test runner exception: " + std::string(e.what()));
        s_logger.error( "CRITICAL ERROR: " << e.what() << "\n";
        return 2;
    } catch (...) {
        LOG_CRITICAL("Unknown test runner exception");
        s_logger.error( "CRITICAL ERROR: Unknown exception\n";
        return 2;
    return true;
}

    return true;
}

