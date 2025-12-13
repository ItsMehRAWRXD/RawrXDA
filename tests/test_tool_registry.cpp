/**
 * @file test_tool_registry.cpp
 * @brief Comprehensive test suite for ToolRegistry with full utility validation
 *
 * Tests cover:
 * - Tool registration and lifecycle
 * - Execution with error handling
 * - Logging and metrics collection
 * - Input/output validation
 * - Caching and retry logic
 * - Configuration management
 * - Batch execution
 * - Health monitoring
 * - Statistics tracking
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <chrono>
#include "tool_registry.hpp"

// ============================================================================
// Mock Logger for Testing
// ============================================================================

class TestLogger : public Logger {
public:
    struct LogEntry {
        std::string level;
        std::string component;
        std::string message;
    };
    
    std::vector<LogEntry> entries;
    
    void debug(const std::string& component, const std::string& message) override {
        entries.push_back({"DEBUG", component, message});
        std::cout << "[DEBUG] " << component << ": " << message << std::endl;
    }
    
    void info(const std::string& component, const std::string& message) override {
        entries.push_back({"INFO", component, message});
        std::cout << "[INFO] " << component << ": " << message << std::endl;
    }
    
    void warn(const std::string& component, const std::string& message) override {
        entries.push_back({"WARN", component, message});
        std::cout << "[WARN] " << component << ": " << message << std::endl;
    }
    
    void error(const std::string& component, const std::string& message) override {
        entries.push_back({"ERROR", component, message});
        std::cout << "[ERROR] " << component << ": " << message << std::endl;
    }
};

// ============================================================================
// Mock Metrics for Testing
// ============================================================================

class TestMetrics : public Metrics {
public:
    std::map<std::string, uint64_t> counters;
    std::map<std::string, std::vector<double>> histograms;
    std::map<std::string, double> gauges;
    
    void incrementCounter(const std::string& name, double value) override {
        counters[name] += static_cast<uint64_t>(value);
    }
    
    void decrementGauge(const std::string& name, double value) override {
        gauges[name] -= value;
    }
    
    void incrementGauge(const std::string& name, double value) override {
        gauges[name] += value;
    }
    
    void recordHistogram(const std::string& name, double value) override {
        histograms[name].push_back(value);
    }
};

// ============================================================================
// Test Fixtures and Helpers
// ============================================================================

struct TestContext {
    std::shared_ptr<TestLogger> logger;
    std::shared_ptr<TestMetrics> metrics;
    std::shared_ptr<ToolRegistry> registry;
    
    TestContext() {
        logger = std::make_shared<TestLogger>();
        metrics = std::make_shared<TestMetrics>();
        registry = std::make_shared<ToolRegistry>(logger, metrics);
    }
};

// ============================================================================
// Test Cases
// ============================================================================

void test_tool_registration() {
    std::cout << "\n========== TEST: Tool Registration ==========" << std::endl;
    
    TestContext ctx;
    
    // Define a simple test tool
    ToolDefinition toolDef;
    toolDef.name = "test_echo";
    toolDef.description = "Echo input back";
    toolDef.category = ToolCategory::Custom;
    toolDef.handler = [](const json& params) {
        json result;
        result["echo"] = params.value("message", "");
        return result;
    };
    
    // Register tool
    bool registered = ctx.registry->registerTool(toolDef);
    assert(registered && "Tool registration failed");
    assert(ctx.registry->hasTool("test_echo") && "Tool not found after registration");
    
    std::cout << "✓ Tool registration successful" << std::endl;
}

void test_tool_execution() {
    std::cout << "\n========== TEST: Tool Execution ==========" << std::endl;
    
    TestContext ctx;
    
    // Register a tool
    ToolDefinition toolDef;
    toolDef.name = "test_add";
    toolDef.description = "Add two numbers";
    toolDef.handler = [](const json& params) {
        double a = params.value("a", 0.0);
        double b = params.value("b", 0.0);
        json result;
        result["sum"] = a + b;
        return result;
    };
    
    ctx.registry->registerTool(toolDef);
    
    // Execute tool
    json params;
    params["a"] = 5.0;
    params["b"] = 3.0;
    
    auto result = ctx.registry->executeTool("test_add", params);
    
    assert(result.success && "Tool execution failed");
    assert(result.data["sum"] == 8.0 && "Tool result incorrect");
    assert(result.executionContext.status == ToolExecutionStatus::Completed);
    
    std::cout << "✓ Tool execution successful: 5 + 3 = " << result.data["sum"] << std::endl;
}

void test_logging_and_metrics() {
    std::cout << "\n========== TEST: Logging and Metrics ==========" << std::endl;
    
    TestContext ctx;
    
    // Register tool
    ToolDefinition toolDef;
    toolDef.name = "test_metrics";
    toolDef.description = "Test metrics collection";
    toolDef.handler = [](const json& params) {
        return json{{"status", "ok"}};
    };
    toolDef.config.enableDetailedLogging = true;
    
    ctx.registry->registerTool(toolDef);
    
    // Execute multiple times
    for (int i = 0; i < 3; ++i) {
        ctx.registry->executeTool("test_metrics", json::object());
    }
    
    // Verify logging
    bool hasInfoLog = false;
    for (const auto& entry : ctx.logger->entries) {
        if (entry.level == "INFO" && entry.message.find("Tool registered") != std::string::npos) {
            hasInfoLog = true;
            break;
        }
    }
    assert(hasInfoLog && "Logging not captured");
    
    // Verify metrics
    assert(ctx.metrics->counters["tools_registered"] == 1 && "Counter not incremented");
    assert(ctx.metrics->counters["tool_executions_total"] == 3 && "Execution counter not correct");
    assert(ctx.metrics->counters["tool_executions_successful"] == 3 && "Success counter not correct");
    
    std::cout << "✓ Logging captured " << ctx.logger->entries.size() << " entries" << std::endl;
    std::cout << "✓ Metrics: " << ctx.metrics->counters["tool_executions_total"] 
              << " total executions, " << ctx.metrics->counters["tool_executions_successful"]
              << " successful" << std::endl;
}

void test_input_validation() {
    std::cout << "\n========== TEST: Input Validation ==========" << std::endl;
    
    TestContext ctx;
    
    // Define tool with validation
    ToolDefinition toolDef;
    toolDef.name = "test_validate";
    toolDef.description = "Tool with input validation";
    toolDef.handler = [](const json& params) {
        return json{{"received", params}};
    };
    
    // Set validation rules
    ToolInputValidation validation;
    validation.requiredFields = {"name", "value"};
    toolDef.inputValidation = validation;
    
    ctx.registry->registerTool(toolDef);
    ctx.registry->setInputValidation("test_validate", validation);
    
    // Test with valid input
    json validParams;
    validParams["name"] = "test";
    validParams["value"] = 42;
    
    auto validResult = ctx.registry->executeTool("test_validate", validParams);
    assert(validResult.success && "Valid input rejected");
    
    // Test with invalid input (missing field)
    json invalidParams;
    invalidParams["name"] = "test";  // missing "value"
    
    auto invalidResult = ctx.registry->executeTool("test_validate", invalidParams);
    assert(!invalidResult.success && "Invalid input accepted");
    assert(invalidResult.error.find("Missing required field") != std::string::npos);
    
    std::cout << "✓ Valid input accepted" << std::endl;
    std::cout << "✓ Invalid input rejected: " << invalidResult.error << std::endl;
}

void test_error_handling_and_retry() {
    std::cout << "\n========== TEST: Error Handling and Retry ==========" << std::endl;
    
    TestContext ctx;
    
    int attemptCount = 0;
    
    // Define tool that fails then succeeds
    ToolDefinition toolDef;
    toolDef.name = "test_retry";
    toolDef.description = "Tool that fails then succeeds";
    toolDef.handler = [&attemptCount](const json& params) {
        attemptCount++;
        if (attemptCount < 3) {
            throw std::runtime_error("Temporary failure");
        }
        return json{{"success", true, "attempts", attemptCount}};
    };
    
    // Enable retry
    toolDef.config.retryOnFailure = true;
    toolDef.config.maxRetries = 3;
    toolDef.config.retryDelayMs = 10;
    
    ctx.registry->registerTool(toolDef);
    
    // Execute (should succeed after retries)
    auto result = ctx.registry->executeTool("test_retry", json::object());
    
    assert(result.success && "Tool failed despite retries");
    assert(result.wasRetried && "Retry was not attempted");
    assert(result.retryCount == 2 && "Retry count incorrect");
    
    std::cout << "✓ Tool succeeded after " << result.retryCount << " retries" << std::endl;
    std::cout << "✓ Total attempts: " << attemptCount << std::endl;
}

void test_caching() {
    std::cout << "\n========== TEST: Caching ==========" << std::endl;
    
    TestContext ctx;
    
    int callCount = 0;
    
    // Define tool with caching
    ToolDefinition toolDef;
    toolDef.name = "test_cache";
    toolDef.description = "Tool with result caching";
    toolDef.handler = [&callCount](const json& params) {
        callCount++;
        return json{{"call_number", callCount}};
    };
    
    toolDef.config.enableCaching = true;
    toolDef.config.cacheValidityMs = 5000;
    
    ctx.registry->registerTool(toolDef);
    ctx.registry->enableCaching("test_cache", 5000);
    
    json params;
    params["test"] = "data";
    
    // First call
    auto result1 = ctx.registry->executeTool("test_cache", params);
    assert(result1.success && "First call failed");
    assert(result1.data["call_number"] == 1);
    assert(!result1.metrics.fromCache);
    
    // Second call (should be cached)
    auto result2 = ctx.registry->executeTool("test_cache", params);
    assert(result2.success && "Second call failed");
    assert(result2.data["call_number"] == 1 && "Cache not used");
    assert(result2.metrics.fromCache && "Cache flag not set");
    assert(callCount == 1 && "Handler was called multiple times");
    
    std::cout << "✓ First call executed (count: " << result1.data["call_number"] << ")" << std::endl;
    std::cout << "✓ Second call cached (count: " << result2.data["call_number"] << ")" << std::endl;
    std::cout << "✓ Handler called only " << callCount << " time(s)" << std::endl;
}

void test_configuration_management() {
    std::cout << "\n========== TEST: Configuration Management ==========" << std::endl;
    
    TestContext ctx;
    
    // Register tool
    ToolDefinition toolDef;
    toolDef.name = "test_config";
    toolDef.description = "Tool for config testing";
    toolDef.handler = [](const json& params) {
        return json{{"ok", true}};
    };
    
    ctx.registry->registerTool(toolDef);
    
    // Test timeout configuration
    ctx.registry->setToolTimeout("test_config", 5000);
    auto tool = ctx.registry->getTool("test_config");
    assert(tool && tool->config.timeoutMs == 5000 && "Timeout not set");
    
    // Test enable/disable
    ctx.registry->setToolEnabled("test_config", false);
    auto result = ctx.registry->executeTool("test_config", json::object());
    assert(result.executionContext.status == ToolExecutionStatus::SkippedByToggle && 
           "Tool not disabled");
    
    // Re-enable
    ctx.registry->setToolEnabled("test_config", true);
    result = ctx.registry->executeTool("test_config", json::object());
    assert(result.success && "Tool not re-enabled");
    
    // Test global timeout
    ctx.registry->setGlobalTimeout(10000);
    
    std::cout << "✓ Timeout configured: 5000ms" << std::endl;
    std::cout << "✓ Tool disabled and re-enabled" << std::endl;
    std::cout << "✓ Global timeout configured: 10000ms" << std::endl;
}

void test_batch_execution() {
    std::cout << "\n========== TEST: Batch Execution ==========" << std::endl;
    
    TestContext ctx;
    
    // Register multiple tools
    for (int i = 1; i <= 3; ++i) {
        ToolDefinition toolDef;
        toolDef.name = "batch_tool_" + std::to_string(i);
        toolDef.description = "Batch test tool " + std::to_string(i);
        int id = i;
        toolDef.handler = [id](const json& params) {
            json result;
            result["tool_id"] = id;
            result["input"] = params;
            return result;
        };
        
        ctx.registry->registerTool(toolDef);
    }
    
    // Execute batch
    std::vector<std::pair<std::string, json>> batch;
    batch.push_back({"batch_tool_1", json::object()});
    batch.push_back({"batch_tool_2", json::object()});
    batch.push_back({"batch_tool_3", json::object()});
    
    auto results = ctx.registry->executeBatch(batch);
    
    assert(results.size() == 3 && "Batch size mismatch");
    for (size_t i = 0; i < results.size(); ++i) {
        assert(results[i].success && "Batch item failed");
        assert(results[i].data["tool_id"] == static_cast<int>(i + 1));
    }
    
    std::cout << "✓ Batch executed with " << results.size() << " tools" << std::endl;
    std::cout << "✓ All batch items successful" << std::endl;
}

void test_statistics_and_monitoring() {
    std::cout << "\n========== TEST: Statistics and Monitoring ==========" << std::endl;
    
    TestContext ctx;
    
    // Register and execute tool multiple times
    ToolDefinition toolDef;
    toolDef.name = "test_stats";
    toolDef.description = "Tool for statistics testing";
    toolDef.handler = [](const json& params) {
        return json{{"status", "ok"}};
    };
    
    ctx.registry->registerTool(toolDef);
    
    // Execute multiple times
    for (int i = 0; i < 5; ++i) {
        ctx.registry->executeTool("test_stats", json::object());
    }
    
    // Get statistics
    auto stats = ctx.registry->getToolStatistics("test_stats");
    assert(stats.contains("tool_name") && stats["tool_name"] == "test_stats");
    assert(stats["total_executions"] == 5);
    assert(stats["successful_executions"] == 5);
    assert(stats["success_rate"] == 1.0);
    
    // Get metrics snapshot
    auto snapshot = ctx.registry->getMetricsSnapshot();
    assert(snapshot.contains("total_executions"));
    assert(snapshot.contains("tools"));
    assert(snapshot["tools"].contains("test_stats"));
    
    // Get health status
    auto health = ctx.registry->getHealthStatus();
    assert(health.contains("status"));
    assert(health["status"] == "HEALTHY");
    
    std::cout << "✓ Tool statistics: " << stats["total_executions"] << " executions, "
              << stats["success_rate"] << " success rate" << std::endl;
    std::cout << "✓ Metrics snapshot captured: " << snapshot["total_executions"] << " total" << std::endl;
    std::cout << "✓ Health status: " << health["status"] << std::endl;
}

void test_execution_context_and_tracing() {
    std::cout << "\n========== TEST: Execution Context and Tracing ==========" << std::endl;
    
    TestContext ctx;
    
    // Register tool
    ToolDefinition toolDef;
    toolDef.name = "test_trace";
    toolDef.description = "Tool for tracing testing";
    toolDef.handler = [](const json& params) {
        return json{{"traced", true}};
    };
    toolDef.config.enableTracing = true;
    
    ctx.registry->registerTool(toolDef);
    
    // Execute with trace context
    auto result = ctx.registry->executeToolWithTrace("test_trace", json::object(),
                                                      "trace-123", "parent-456");
    
    assert(!result.executionContext.executionId.empty() && "Execution ID not set");
    assert(result.executionContext.traceId == "trace-123" && "Trace ID not set");
    assert(result.executionContext.parentSpanId == "parent-456" && "Parent span ID not set");
    assert(!result.executionContext.spanId.empty() && "Span ID not generated");
    
    std::cout << "✓ Execution ID: " << result.executionContext.executionId.substr(0, 8) << "..." << std::endl;
    std::cout << "✓ Trace ID: " << result.executionContext.traceId << std::endl;
    std::cout << "✓ Parent Span ID: " << result.executionContext.parentSpanId << std::endl;
    std::cout << "✓ Span ID: " << result.executionContext.spanId.substr(0, 8) << "..." << std::endl;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════╗
║              ToolRegistry Comprehensive Test Suite                     ║
║              Full Utility with Production Readiness                    ║
╚════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    try {
        // Run all tests
        test_tool_registration();
        test_tool_execution();
        test_logging_and_metrics();
        test_input_validation();
        test_error_handling_and_retry();
        test_caching();
        test_configuration_management();
        test_batch_execution();
        test_statistics_and_monitoring();
        test_execution_context_and_tracing();
        
        std::cout << R"(
╔════════════════════════════════════════════════════════════════════════╗
║                      ✓ ALL TESTS PASSED                               ║
║                  ToolRegistry is production-ready                      ║
╚════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
