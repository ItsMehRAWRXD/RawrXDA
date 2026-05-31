#pragma once
// ============================================================================
// Enhanced Tool Registry for Agentic Inference
// 
// CRITICAL FIX FOR: BackendError on SubmitInference
// Problem: Tool registry not injected into AIImplementation inference path
// Solution: Registry-aware bridge with structured output enforcement
// Impact: Unblocks tool execution loop and 44-tool ecosystem
// ============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Tool Execution Result
// ============================================================================

struct ToolExecutionResult {
    bool success = false;
    std::string output;
    std::string error;
    int exitCode = 0;
    uint64_t durationMs = 0;
};

// ============================================================================
// Tool Schema Definition
// ============================================================================

struct ToolSchema {
    std::string name;
    std::string description;
    nlohmann::json parameters;
    std::vector<std::string> required;
};

// ============================================================================
// Enhanced Tool Registry
// ============================================================================

class ToolRegistry {
public:
    // Singleton instance
    static ToolRegistry& Instance();
    
    // Initialization
    bool Initialize();
    bool IsInitialized() const;
    void Shutdown();
    
    // Tool registration
    void RegisterTool(const std::string& name, 
                      const std::string& description,
                      const nlohmann::json& parameters,
                      const std::vector<std::string>& required,
                      std::function<ToolExecutionResult(const nlohmann::json&)> executor);
    
    void RegisterSimpleTool(const std::string& name,
                           const std::string& description,
                           std::function<std::string(const std::string&)> executor);
    
    // Tool execution
    ToolExecutionResult ExecuteTool(const std::string& name, const nlohmann::json& params);
    bool ExecuteToolByName(const std::string& name,
                          const std::string& jsonParams,
                          std::string& outResult);
    
    // Tool discovery
    std::vector<ToolSchema> GetToolSchemas() const;
    std::vector<std::string> GetToolNames() const;
    size_t GetToolCount() const;
    bool HasTool(const std::string& name) const;
    std::string ListTools() const;
    
    // Tool validation
    bool ValidateToolParams(const std::string& name, const nlohmann::json& params) const;
    
private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    
    struct ToolEntry {
        ToolSchema schema;
        std::function<ToolExecutionResult(const nlohmann::json&)> executor;
        std::function<std::string(const std::string&)> simpleExecutor;
        bool isSimple = false;
    };
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ToolEntry> m_tools;
    std::atomic<bool> m_initialized{false};
    
    void RegisterCoreTools();
    void RegisterFileTools();
    void RegisterCodeTools();
    void RegisterSystemTools();
};

// ============================================================================
// C API for Tool Registry (DLL Export)
// ============================================================================

extern "C" {
    __declspec(dllexport) void* __stdcall InitializeToolRegistry();
    __declspec(dllexport) int __stdcall ExecuteToolByName(
        const char* toolName,
        const char* jsonParams,
        char* resultBuffer,
        uint64_t bufferSize);
    __declspec(dllexport) int __stdcall GetToolCount();
    __declspec(dllexport) int __stdcall GetToolSchemas(
        char* schemaBuffer,
        uint64_t bufferSize);
}

} // namespace Agentic
} // namespace RawrXD
