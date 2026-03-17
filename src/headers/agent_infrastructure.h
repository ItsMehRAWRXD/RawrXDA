#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

// ============================================================================
// CATEGORY 2: Agent Infrastructure (~25+ symbols)
// ============================================================================

namespace RawrXD {
namespace Agent {

// Forward declarations
struct AgentLoopConfig;
struct EditorContext;
struct FIMBuildResult;
struct LLMChatRequest;
struct LLMChatResponse;
struct AgentSession;
struct AgentStep;
struct InferenceResult;

// ============================================================================
// RawrXD::Agent::BoundedAgentLoop
// ============================================================================

class BoundedAgentLoop {
public:
    BoundedAgentLoop();
    ~BoundedAgentLoop();
    
    BoundedAgentLoop(const BoundedAgentLoop&) = delete;
    BoundedAgentLoop& operator=(const BoundedAgentLoop&) = delete;

    std::string Execute(const std::string& prompt);
    void ExecuteAsync(const std::string& prompt);
    
    void Configure(const AgentLoopConfig& config);
    void SetProgressCallback(
        std::function<void(int current, int total, const std::string& status, const std::string& detail)> callback);
    
    void SetLLMBackend(std::function<LLMChatResponse(const LLMChatRequest&)> backend);
};

// ============================================================================
// RawrXD::Agent::FIMPromptBuilder
// ============================================================================

struct FIMBuildResult {
    std::string prompt;
    uint32_t token_count;
    bool success;
    std::string error_message;
};

class FIMPromptBuilder {
public:
    FIMPromptBuilder();
    ~FIMPromptBuilder();
    
    FIMPromptBuilder(const FIMPromptBuilder&) = delete;
    FIMPromptBuilder& operator=(const FIMPromptBuilder&) = delete;

    FIMBuildResult Build(const EditorContext& context) const;
    static int EstimateTokens(const std::string& text);
};

// ============================================================================
// AgenticObservability
// ============================================================================

class AgenticObservability {
public:
    struct TimingGuard {
    public:
        explicit TimingGuard(const std::string& span_name);
        ~TimingGuard();
        
    private:
        std::string span_name_;
        uint64_t start_time_;
    };

    AgenticObservability();
    ~AgenticObservability();
    
    AgenticObservability(const AgenticObservability&) = delete;
    AgenticObservability& operator=(const AgenticObservability&) = delete;

    void incrementCounter(const std::string& counter_name, int delta, 
                          const nlohmann::json& tags = nlohmann::json());
    void logInfo(const std::string& module, const std::string& message, 
                 const nlohmann::json& context = nlohmann::json());
    void logError(const std::string& module, const std::string& message, 
                  const nlohmann::json& context = nlohmann::json());
    void logWarn(const std::string& module, const std::string& message, 
                 const nlohmann::json& context = nlohmann::json());
    void logDebug(const std::string& module, const std::string& message, 
                  const nlohmann::json& context = nlohmann::json());
    
    void recordHistogram(const std::string& histogram_name, float value, 
                         const nlohmann::json& tags = nlohmann::json());
    void setGauge(const std::string& gauge_name, float value, 
                  const nlohmann::json& tags = nlohmann::json());
    
    std::unique_ptr<TimingGuard> measureDuration(const std::string& span_name);
    
    std::string startSpan(const std::string& span_name, const std::string& parent_span);
    void endSpan(const std::string& span_id, bool success, const std::string& error_msg, int status_code);
};

// ============================================================================
// SubsystemRegistry
// ============================================================================

enum class SubsystemId : unsigned char {
    Subsys_Scheduler = 0,
    Subsys_GPU = 1,
    Subsys_Conflict = 2,
    Subsys_Heartbeat = 3,
    Subsys_DMA = 4,
    Subsys_Max = 5
};

struct SubsystemParams {
    SubsystemId subsystem_id;
    uint32_t operation_code;
    void* context;
    uint32_t context_size;
};

struct SubsystemResult {
    bool success;
    std::string error_message;
    uint64_t execution_time_us;
};

class SubsystemRegistry {
public:
    SubsystemRegistry(const SubsystemRegistry&) = delete;
    SubsystemRegistry& operator=(const SubsystemRegistry&) = delete;

    static SubsystemRegistry& instance();
    
    SubsystemResult invoke(const SubsystemParams& params);
    bool isAvailable(SubsystemId subsystem_id) const;
    const char* getSwitchName(SubsystemId subsystem_id) const;

private:
    SubsystemRegistry();
    ~SubsystemRegistry();
};

// ============================================================================
// RawrXD::Agent::OrchestratorBridge
// ============================================================================

class OrchestratorBridge {
public:
    OrchestratorBridge();
    ~OrchestratorBridge();
    
    OrchestratorBridge(const OrchestratorBridge&) = delete;
    OrchestratorBridge& operator=(const OrchestratorBridge&) = delete;

    bool Initialize(const std::string& config_path, const std::string& model_path);
    void RunAgentAsync(const std::string& prompt);

private:
    void WireToolHandlers();
};

// ============================================================================
// Supporting structures for Agent
// ============================================================================

struct AgentLoopConfig {
    uint32_t max_iterations;
    uint32_t max_retries;
    uint32_t timeout_ms;
    std::string model_name;
    float temperature;
};

struct EditorContext {
    std::string file_path;
    std::string file_content;
    uint32_t cursor_line;
    uint32_t cursor_column;
    std::string selected_text;
};

struct LLMChatRequest {
    std::string prompt;
    std::vector<std::string> conversation_history;
    float temperature;
    uint32_t max_tokens;
};

struct LLMChatResponse {
    std::string response;
    uint32_t tokens_used;
    bool success;
    std::string error_message;
};

struct AgentSession {
    std::string session_id;
    std::vector<AgentStep> steps;
    bool completed;
    std::string final_result;
    uint64_t elapsed_ms;
};

struct AgentStep {
    uint32_t step_number;
    std::string action;
    std::string observation;
    std::string thought;
};

struct InferenceResult {
    std::string content;
    std::vector<std::string> tool_calls;
    bool success;
    std::string error_message;
};

// ============================================================================
// RawrXD::Agent::DiskRecoveryToolHandler
// ============================================================================

class AgentToolRegistry;

class DiskRecoveryToolHandler {
public:
    static void RegisterTools(AgentToolRegistry& registry);
};

}  // namespace Agent
}  // namespace RawrXD

// ============================================================================
// RawrXD::Agent::AgentToolRegistry
// ============================================================================

namespace RawrXD {
namespace Agent {

struct ToolExecResult {
    std::string output;
    bool success;
    int error_code;
};

class AgentToolRegistry {
public:
    ToolExecResult Dispatch(const std::string& tool_name, const nlohmann::json& params);
};

}  // namespace Agent
}  // namespace RawrXD
