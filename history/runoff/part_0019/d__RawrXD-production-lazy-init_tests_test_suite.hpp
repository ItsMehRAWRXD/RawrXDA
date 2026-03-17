// ============================================================================
// Test Suite for Phase 4: CLI→GUI Command Implementation
// ============================================================================
// Comprehensive tests for command processing, IPC communication, and integration
// ============================================================================

#pragma once

#include "../src/gui/command_handlers.hpp"
#include "../src/gui/command_registry.hpp"
#include "../src/gui/ipc_server.hpp"
#include "../src/cli/cli_ipc_client.hpp"
#include "../src/cli/cli_command_integration.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Test {

// ============================================================================
// TEST CONFIGURATION
// ============================================================================

struct TestConfig {
    bool verbose = false;
    bool show_progress = true;
    std::string server_name = "rawrxd-test";
    int timeout_ms = 5000;
    bool auto_reconnect = true;
    int max_test_attempts = 3;
};

// ============================================================================
// TEST RESULTS
// ============================================================================

struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
    double duration_ms;
    std::vector<std::string> details;
    
    TestResult(const std::string& name) 
        : test_name(name), passed(false), duration_ms(0) {}
};

class TestResults {
public:
    void addResult(const TestResult& result);
    void addResult(const std::string& test_name, bool passed, 
                   const std::string& error = "");
    
    size_t getTotalCount() const { return results_.size(); }
    size_t getPassedCount() const { return passed_count_; }
    size_t getFailedCount() const { return failed_count_; }
    size_t getErrorCount() const { return error_count_; }
    
    double getTotalDuration() const { return total_duration_ms_; }
    double getAverageDuration() const { 
        return results_.empty() ? 0 : total_duration_ms_ / results_.size(); 
    }
    
    void printSummary() const;
    void printDetails() const;
    void printFailedTests() const;
    
    bool allPassed() const { return failed_count_ == 0 && error_count_ == 0; }
    
private:
    std::vector<TestResult> results_;
    size_t passed_count_ = 0;
    size_t failed_count_ = 0;
    size_t error_count_ = 0;
    double total_duration_ms_ = 0;
};

// ============================================================================
// TEST BASE CLASS
// ============================================================================

class TestBase {
public:
    TestBase(const std::string& test_name, const TestConfig& config);
    virtual ~TestBase() = default;
    
    // Non-copyable
    TestBase(const TestBase&) = delete;
    TestBase& operator=(const TestBase&) = delete;
    
    // Test execution
    virtual TestResult run() = 0;
    
    // Configuration
    void setVerbose(bool verbose) { config_.verbose = verbose; }
    void setShowProgress(bool show_progress) { config_.show_progress = show_progress; }
    
    // Test information
    std::string getTestName() const { return test_name_; }
    TestConfig getConfig() const { return config_; }
    
protected:
    // Utility methods
    void log(const std::string& message);
    void logError(const std::string& message);
    void logSuccess(const std::string& message);
    
    void sleep(int milliseconds);
    bool waitForCondition(std::function<bool()> condition, 
                         int timeout_ms, int interval_ms = 100);
    
    // Test assertions
    bool assertTrue(bool condition, const std::string& message);
    bool assertFalse(bool condition, const std::string& message);
    bool assertEqual(const std::string& expected, const std::string& actual, 
                    const std::string& message);
    bool assertEqual(int expected, int actual, const std::string& message);
    bool assertEqual(size_t expected, size_t actual, const std::string& message);
    bool assertNotEmpty(const std::string& str, const std::string& message);
    bool assertEmpty(const std::string& str, const std::string& message);
    
private:
    std::string test_name_;
    TestConfig config_;
};

// ============================================================================
// COMMAND REGISTRY TESTS
// ============================================================================

class CommandRegistryTest : public TestBase {
public:
    CommandRegistryTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testCommandRegistration();
    bool testCommandRetrieval();
    bool testCommandAliases();
    bool testCommandValidation();
    bool testCommandMetadata();
    bool testInvalidCommandHandling();
};

// ============================================================================
// COMMAND HANDLERS TESTS
// ============================================================================

class CommandHandlersTest : public TestBase {
public:
    CommandHandlersTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testLoadModelCommand();
    bool testUnloadModelCommand();
    bool testFocusPaneCommand();
    bool testShowChatCommand();
    bool testHideChatCommand();
    bool testGetStatusCommand();
    bool testExecuteCommand();
    bool testBatchOperations();
    bool testErrorHandling();
    bool testCallbacks();
};

// ============================================================================
// IPC CLIENT TESTS
// ============================================================================

class CLIIPCClientTest : public TestBase {
public:
    CLIIPCClientTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testConnection();
    bool testCommandSending();
    bool testResponseHandling();
    bool testErrorHandling();
    bool testReconnection();
    bool testTimeoutHandling();
    bool testStatistics();
    bool testCallbacks();
    
    std::unique_ptr<CLIIPCClient> client_;
};

// ============================================================================
// IPC SERVER TESTS
// ============================================================================

class GUIIPCServerTest : public TestBase {
public:
    GUIIPCServerTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testServerStartup();
    bool testCommandProcessing();
    bool testConnectionManagement();
    bool testResponseSending();
    bool testErrorHandling();
    bool testStatistics();
    bool testCallbacks();
    
    std::unique_ptr<GUIIPCServer> server_;
};

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

class CommandIntegrationTest : public TestBase {
public:
    CommandIntegrationTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testInitialization();
    bool testCommandExecution();
    bool testHighLevelOperations();
    bool testBatchOperations();
    bool testErrorRecovery();
    bool testConfiguration();
    bool testStatistics();
    
    std::unique_ptr<CLICommandIntegration> integration_;
    std::unique_ptr<GUIIPCServer> server_;
};

// ============================================================================
// END-TO-END TESTS
// ============================================================================

class EndToEndTest : public TestBase {
public:
    EndToEndTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testCompleteCommandFlow();
    bool testMultipleCommands();
    bool testErrorScenarios();
    bool testReconnectionScenario();
    bool testBatchCommandFlow();
    bool testConcurrentCommands();
    
    std::unique_ptr<GUIIPCServer> server_;
    std::unique_ptr<CLIIPCClient> client_;
};

// ============================================================================
// TEST RUNNER
// ============================================================================

class TestRunner {
public:
    TestRunner(const TestConfig& config);
    ~TestRunner();
    
    // Test execution
    bool runAllTests();
    bool runTest(const std::string& test_name);
    bool runTestCategory(const std::string& category);
    
    // Test registration
    void registerTest(std::unique_ptr<TestBase> test);
    void registerTestCategory(const std::string& category, 
                             std::vector<std::unique_ptr<TestBase>> tests);
    
    // Results
    TestResults getResults() const { return results_; }
    void printResults() const;
    
    // Configuration
    void setConfig(const TestConfig& config) { config_ = config; }
    TestConfig getConfig() const { return config_; }
    
private:
    TestConfig config_;
    TestResults results_;
    std::vector<std::unique_ptr<TestBase>> tests_;
    std::map<std::string, std::vector<TestBase*>> categories_;
    
    void setupTests();
    void setupCategories();
    TestResult runTestInternal(TestBase* test);
};

// ============================================================================
// TEST UTILITIES
// ============================================================================

class TestUtils {
public:
    // Server management
    static std::unique_ptr<GUIIPCServer> createTestServer(const TestConfig& config);
    static std::unique_ptr<CLIIPCClient> createTestClient(const TestConfig& config);
    static std::unique_ptr<CLICommandIntegration> createTestIntegration(const TestConfig& config);
    
    // Command utilities
    static std::string generateRandomString(size_t length);
    static std::vector<std::string> generateRandomCommands(size_t count);
    static std::string createTempModelPath();
    static void cleanupTempModel(const std::string& path);
    
    // Timing utilities
    static double measureExecutionTime(std::function<void()> func);
    static bool waitForServer(const std::string& server_name, int timeout_ms);
    static bool waitForClientConnection(CLIIPCClient* client, int timeout_ms);
    
    // Mock utilities
    static std::unique_ptr<RawrXDMainWindow> createMockMainWindow();
    static std::unique_ptr<ModelLoaderWidget> createMockModelLoader();
    static std::unique_ptr<AgentChatPane> createMockChatPane();
    static std::unique_ptr<TerminalManager> createMockTerminalManager();
};

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

class PerformanceTest : public TestBase {
public:
    PerformanceTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testCommandThroughput();
    bool testConnectionLatency();
    bool testMessageSerialization();
    bool testBatchOperationPerformance();
    bool testConcurrentCommandPerformance();
    bool testMemoryUsage();
    
    void measureCommandThroughput();
    void measureConnectionLatency();
    void measureBatchPerformance();
    void measureConcurrentPerformance();
};

// ============================================================================
// STRESS TESTS
// ============================================================================

class StressTest : public TestBase {
public:
    StressTest(const TestConfig& config);
    
    TestResult run() override;
    
private:
    bool testHighCommandVolume();
    bool testRapidConnections();
    bool testConnectionFlooding();
    bool testResourceExhaustion();
    bool testLongRunningOperations();
    bool testErrorRecoveryUnderLoad();
    
    void generateHighCommandVolume();
    void createRapidConnections();
    void floodWithConnections();
    void exhaustResources();
    void runLongOperations();
    void testRecoveryUnderLoad();
};

// ============================================================================
// GLOBAL TEST FUNCTIONS
// ============================================================================

/**
 * @brief Run all tests with default configuration
 * @return true if all tests passed, false otherwise
 */
bool runAllTests();

/**
 * @brief Run all tests with custom configuration
 * @param config Test configuration
 * @return true if all tests passed, false otherwise
 */
bool runAllTests(const TestConfig& config);

/**
 * @brief Run a specific test by name
 * @param test_name Name of the test to run
 * @param config Test configuration
 * @return true if test passed, false otherwise
 */
bool runTest(const std::string& test_name, const TestConfig& config);

/**
 * @brief Run all tests in a category
 * @param category Test category name
 * @param config Test configuration
 * @return true if all tests in category passed, false otherwise
 */
bool runTestCategory(const std::string& category, const TestConfig& config);

// ============================================================================
// TEST MAIN ENTRY POINT
// ============================================================================

/**
 * @brief Main entry point for test suite
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code (0 = all passed, 1 = some failed, 2 = error)
 */
int testMain(int argc, char* argv[]);

} // namespace Test
} // namespace RawrXD