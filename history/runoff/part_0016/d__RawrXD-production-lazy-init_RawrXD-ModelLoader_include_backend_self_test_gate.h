#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations to avoid circular dependencies
namespace RawrXD {
namespace Backend {

struct OllamaChatMessage;
struct OllamaChatRequest;
struct OllamaChatResponse;
struct ToolResult;
class OllamaClient;
class AgenticTool;
class AgenticToolExecutor;
class ToolRegistry;
class PlanOrchestrator;
class ZeroDayAgenticEngine;
class UniversalModelRouter;
class HotpatchSystem;
class SelfTestGate;

// Chat configuration
struct ChatConfig {
    std::string model;
    int max_tokens;
    float temperature;
    int max_tool_iterations;
    std::function<void(const std::string&)> on_message;
    std::function<void(const std::string&)> on_tool_call;
    
    ChatConfig() : model(""), max_tokens(512), temperature(0.7f), max_tool_iterations(10) {}
    ChatConfig(const std::string& m, int tokens, float temp)
        : model(m), max_tokens(tokens), temperature(temp), max_tool_iterations(10) {}
};

// Self-test gate for system validation
class SelfTestGate {
public:
    SelfTestGate();
    ~SelfTestGate();
    
    // Initialize the self-test system
    bool Initialize();
    
    // Run all self-tests
    bool RunAllTests();
    
    // Run diagnostics
    bool RunDiagnostics();
    
    // Run specific test category
    bool RunTestCategory(const std::string& category);
    
    // Get test results
    std::string GetTestResults() const;
    
    // Get failure report
    std::string GetFailureReport() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD